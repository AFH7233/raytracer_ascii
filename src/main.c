#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <fcntl.h>

const char *ramp = ".'`^\",:;Il!i~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";

typedef struct {
  double x, y, z, w;
} vec4;

typedef struct {
  int r, g, b;
} color_t;

vec4 add(vec4 a, vec4 b) {
  return (vec4){.x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z, .w = a.w};
}

vec4 sub(vec4 a, vec4 b) {
  return (vec4){.x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z, .w = a.w};
}

vec4 scale(double k, vec4 a) {
  return (vec4){.x = k * a.x, .y = k * a.y, .z = k * a.z, .w = a.w};
}

double dot(vec4 a, vec4 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

vec4 cross(vec4 a, vec4 b) {
  return (vec4){
    .x = a.y * b.z - a.z * b.y,
    .y = a.z * b.x - a.x * b.z,
    .z = a.x * b.y - a.y * b.x,
    .w = 1.0
  };
}

vec4 normalize(vec4 a) {
  double mag = dot(a, a);
  if (mag < 0.000001) return a;
  return scale(1.0 / sqrt(mag), a);
}

double intersect_sphere(vec4 eye, vec4 dir, vec4 center, double radius) {
  vec4 oc = sub(eye, center);
  double a = dot(dir, dir);
  double b = 2.0 * dot(oc, dir);
  double c = dot(oc, oc) - radius * radius;
  double discriminant = b * b - 4.0 * a * c;
  if (discriminant < 0.0) return -1.0;
  double t = (-b - sqrt(discriminant)) / (2.0 * a);
  if (t > 0.0001) return t;
  return -1.0;
}

double intersect_plane(vec4 eye, vec4 dir, double height) {
  if (fabs(dir.y) < 0.000001) return -1.0;
  double t = (height - eye.y) / dir.y;
  if (t > 0.0001) return t;
  return -1.0;
}

struct termios orig_termios;
void disableRawMode(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
  printf("\x1b[?25h");
}
void enableRawMode(void) {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);
  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  printf("\x1b[?25l");
}

int main(void) {
  enableRawMode();
  int H, W;
  struct winsize w;
  bool exit = false;

  vec4 eye = {.x = 0.0, .y = 0.0, .z = 1.0, .w = 1.0};
  vec4 target = {.x = 0.0, .y = 0.0, .z = -3.0, .w = 1.0};
  vec4 world_up = {.x = 0.0, .y = 1.0, .z = 0.0, .w = 1.0};

  bool render = true;

  vec4 sphere1_c = {.x = 0.0, .y = 0.0, .z = -3.0, .w = 1.0};
  double sphere1_r = 2.0;
  color_t color1 = {220, 50, 50};

  vec4 sphere2_c = {.x = -3.0, .y = 1.0, .z = -4.0, .w = 1.0};
  double sphere2_r = 1.0;
  color_t color2 = {50, 100, 255};
  double plane_y = -2.0;
  vec4 light_pos = {.x = -5.0, .y = 5.0, .z = 5.0, .w = 1.0};


  char *framebuffer = malloc(sizeof(char)); //temp size
  size_t fb_size = 0;

  while (!exit) {
    fb_size = H * W * 32;
    framebuffer = realloc(framebuffer, fb_size);
    char *ptr = framebuffer;

    vec4 f = normalize(sub(target, eye));
    vec4 r = normalize(cross(f, world_up));
    vec4 true_up = normalize(cross(r, f));

    if (render) {
      if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_row == 0) {
        H = 24;
        W = 80;
      } else {
        H = w.ws_row;
        W = w.ws_col;
      }

      double char_aspect = 0.5;
      double aspect = ((double)W / H) * char_aspect;
      ptr += sprintf(ptr, "\x1b[H");

      for (int y = 0; y < H; y++) {
        ptr += sprintf(ptr, "\x1b[%d;1H", y + 1);
        for (int x = 0; x < W; x++) {
          double u = (((2.0 * (x + 0.5)) / W) - 1.0) * aspect;
          double v = 1.0 - ((2.0 * (y + 0.5)) / H);

          vec4 local_dir = add(f, add(scale(u, r), scale(v, true_up)));
          vec4 dir = normalize(local_dir);

          double t_min = 1e9;
          int hit_obj = 0;

          double t1 = intersect_sphere(eye, dir, sphere1_c, sphere1_r);
          if (t1 > 0.0 && t1 < t_min) {
            t_min = t1;
            hit_obj = 1;
          }

          double t2 = intersect_sphere(eye, dir, sphere2_c, sphere2_r);
          if (t2 > 0.0 && t2 < t_min) {
            t_min = t2;
            hit_obj = 2;
          }

          double t3 = intersect_plane(eye, dir, plane_y);
          if (t3 > 0.0 && t3 < t_min) {
            t_min = t3;
            hit_obj = 3;
          }

          if (hit_obj == 0) {
            ptr += sprintf(ptr, "\x1b[48;2;135;206;235m ");
            continue;
          }

          vec4 hit = add(eye, scale(t_min, dir));
          vec4 normal;
          color_t obj_color;
          double shininess = 32.0;

          if (hit_obj == 1) {
            normal = normalize(sub(hit, sphere1_c));
            obj_color = color1;
          } else if (hit_obj == 2) {
            normal = normalize(sub(hit, sphere2_c));
            obj_color = color2;
          } else {
            normal = (vec4){.x = 0.0, .y = 1.0, .z = 0.0, .w = 0.0};
            // Cuadritos
            int cx = (int) floor(hit.x * 0.5);
            int cz = (int) floor(hit.z * 0.5);
            if ((cx + cz) & 1) {
              obj_color = (color_t){180, 180, 180};
            } else {
              obj_color = (color_t){60, 60, 50};
            }
            shininess = 6.0;
          }

          vec4 light_dir = normalize(sub(light_pos, hit));
          vec4 view_dir = normalize(sub(eye, hit));
          vec4 mag_light = sub(light_pos, hit);
          double light_dist = sqrt(dot(mag_light, mag_light));
          double ambient = 0.15;

          double diff = dot(normal, light_dir);
          if (diff < 0.0) diff = 0.0;
          double diffuse = 0.7 * diff;

          double ndotl = dot(normal, light_dir);
          double specular = 0.0;
          if (ndotl > 0.0) {
            vec4 reflect_dir = sub(scale(2.0 * ndotl, normal), light_dir);
            reflect_dir = normalize(reflect_dir);
            double spec_angle = dot(reflect_dir, view_dir);
            if (spec_angle > 0.0) {
              specular = 0.5 * pow(spec_angle, shininess);
            }
          }

          double shadow_t_min = 1e9;
          vec4 shadow_origin = add(hit, scale(0.001, normal));
          double st1 = intersect_sphere(shadow_origin, light_dir, sphere1_c, sphere1_r);
          if (st1 > 0.0 && st1 < shadow_t_min) shadow_t_min = st1;
          double st2 = intersect_sphere(shadow_origin, light_dir, sphere2_c, sphere2_r);
          if (st2 > 0.0 && st2 < shadow_t_min) shadow_t_min = st2;

          if (shadow_t_min < light_dist) {
            diffuse = 0.0;
            specular = 0.0;
          }

          double total_intensity = ambient + diffuse + specular;
          if (total_intensity > 1.0) total_intensity = 1.0;

          int ramp_len = strlen(ramp);
          int idx = (int)(total_intensity * (ramp_len - 1));
          if (idx < 0) idx = 0;
          if (idx >= ramp_len) idx = ramp_len - 1;
          char pixel = ramp[idx];

          int R = (int) (obj_color.r * total_intensity);
          int G = (int) (obj_color.g * total_intensity);
          int B = (int) (obj_color.b * total_intensity);

          if (R > 255) R = 255;
          if (R < 0) R = 0;
          if (G > 255) G = 255;
          if (G < 0) G = 0;
          if (B > 255) B = 255;
          if (B < 0) B = 0;

          double noise = ((x + y) % 2) ? 0.03 : -0.03;
          double bg_intensity = total_intensity + noise;

          int BR = (int)(R * bg_intensity * 0.3);
          int BG = (int)(G * bg_intensity * 0.3);
          int BB = (int)(B * bg_intensity * 0.3);

          if (BR > 255) BR = 255;
          if (BR < 0) BR = 0;
          if (BG > 255) BG = 255;
          if (BG < 0) BG = 0;
          if (BB > 255) BB = 255;
          if (BB < 0) BB = 0;

          ptr += sprintf(ptr,
            "\x1b[48;2;%d;%d;%dm\x1b[38;2;%d;%d;%dm%c",
            BR, BG, BB, R, G, B, pixel);
        }
      }
      write(STDOUT_FILENO, framebuffer, ptr - framebuffer);
      render = false;
    }

    char key = getchar();
    double step = 0.3;

    switch (key) {
      case 'q':
        exit = true;
        break;
      case 'w':
        eye = add(eye, scale(step, f));
        target = add(target, scale(step, f));
        render = true;
        break;
      case 's':
        eye = sub(eye, scale(step, f));
        target = sub(target, scale(step, f));
        render = true;
        break;
      case 'a':
        eye = sub(eye, scale(step, r));
        target = sub(target, scale(step, r));
        render = true;
        break;
      case 'd':
        eye = add(eye, scale(step,r));
        target = add(target, scale(step,r));
        render = true;
        break;
      default:
        break;
    }
  }
  printf("\x1b[2J\x1b[H\x1b[0m");
  free(framebuffer);
  return 0;
}
