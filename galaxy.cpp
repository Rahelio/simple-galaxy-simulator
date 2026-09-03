#include <stdio.h>
#include <gtk/gtk.h>
#include <math.h>

#define NUM_STARS 700
#define SOFTENING 100 // keeps forces sane near the core
#define gravity_constant 10.0

static int halo_count = NUM_STARS * 0.05;
static int bulge_count = NUM_STARS * 0.15;


// Star-struc(t)
struct Star {
    double x, y;   // Position relative to center
    double vx, vy; // Velocity vectors
}

static Star stars[NUM_STARS];

static void update_star_physics(Star &star, double v0, double dt) {
    // 1. Calculate distance vector to center (0,0)
    const double dx = -star.x;
    const double dy = -star.y;
    const double r_sq = dx*dx + dy*dy;
    const double r = sqrt(r_sq);

    // 2. Gravitational acceleration towards center
    const double accel = v0 * v0 / (r + SOFTENING);


    // 3. Acceleration components
    const double ax = accel * (dx / r);
    const double ay = accel * (dy / r);

    // 4. Euler integration: Update velocity, then position
    star.vx += ax * dt;
    star.vy += ay * dt;
    star.x  += star.vx * dt;
    star.y  += star.vy * dt;
}

static void init_star_orbit(Star &star, double star_x, double star_y, double v0) {
    star.x = star_x;
    star.y = star_y;

    // 1. Calculate radial distance from center (0,0)
    double r = std::sqrt(star.x * star.x + star.y * star.y);

    if (r < 0.001) return; // Prevent division by zero at origin

    // 2. Calculate required circular orbital speed
    double v_speed = v0 * std::sqrt(r / (r + SOFTENING));


    // 3. Assign perpendicular velocity components (-y/r, x/r)
    star.vx = -v_speed * (star.y / r);
    star.vy =  v_speed * (star.x / r);
}

static void drawing_stars(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
  //black background
  cairo_set_source_rgb(cr, 0,0,0);
  cairo_paint(cr);

    for (int i = 0; i < NUM_STARS; i++) {
        double screen_x = width / 2.0 + stars[i].x;
        double screen_y = height / 2.0 + stars[i].y;
        double radius = 2.0;

        double r = sqrt(stars[i].x * stars[i].x + stars[i].y * stars[i].y);
        int falloff_scale = 100.0;
        double brightness = exp(-r / falloff_scale);
        if (brightness < 0.15) brightness = 0.15;

        double color_r, color_g, color_b;

        if (i < bulge_count) {
            // older population: warm yellow/orange
            color_r = 1.0; color_g = 0.8; color_b = 0.5;
        } else if (i < bulge_count + halo_count) {
            // oldest, sparsest population: dull, desaturated red
            color_r = 1.0; color_g = 0.6; color_b = 0.4;
        } else {
            // younger population: cool blue-white
            color_r = 0.7; color_g = 0.8; color_b = 1.0;
        }

        cairo_set_source_rgba(cr, color_r, color_g, color_b, brightness);
        cairo_arc(cr, screen_x, screen_y, radius, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}

//   scale_length - controls how fast density falls off. Small = tight,
//                  bright core with a sparse outer disk. Large = more
//                  evenly spread out. Try something like a third of
//                  max_radius as a starting point.
//   min_radius   - smallest radius allowed (keeps stars off the center point)
//   max_radius   - largest radius allowed
static double sample_exponential_radius(double scale_length, double min_radius, double max_radius) {
    double r;

    // Keep re-rolling until we land inside [min_radius, max_radius].
    // Since the exponential curve rarely produces large values, this
    // loop usually only runs once or twice.
    do {
        // u is a plain uniform random number between 0 and 1.
        const double u = g_random_double_range(0.0, 1.0);

        // This reshapes the uniform number "u" into one that follows an
        // exponential distribution: P(r) ~ e^(-r / scale_length).
        // "std::log(1 - u)" is the natural log (ln) of (1 - u); the minus
        // sign in front flips it positive since ln of a number under 1
        // is always negative.
        r = -scale_length * std::log(1.0 - u);
    } while (r < min_radius || r > max_radius);

    return r;
}

static void get_bulge_position(double scale_length, double min_radius, double max_radius,
                         double &out_x, double &out_y) {
    double radius = sample_exponential_radius(scale_length, min_radius, max_radius);
    double theta  = g_random_double_range(0, 2 * M_PI);

    out_x = radius * cos(theta);
    out_y = radius * sin(theta);
}

// Milky way like spiraled arms
void get_spiral_position(int arm_index, int num_arms, double min_radius, double max_radius, double winding_b, double scatter, double &out_x, double &out_y) {

  // Pick how far from the center this star lands.
    double radius = sample_exponential_radius(max_radius / 3.0, min_radius, max_radius);


    // The logarithmic spiral is r = a * e^(b*theta). We know r, so we solve
    // for theta instead: theta = ln(r / a) / b.
    // "std::log" here means the *natural* log (base e),
    // "a" is just a reference radius so the ratio r/a starts near 1.
    double a = min_radius;
    double theta = std::log(radius / a) / winding_b;

    // Rotate this star onto its assigned arm. With num_arms evenly spaced
    // arms, each one is offset by (2*PI / num_arms) radians from the last.
    theta += arm_index * (2.0 * M_PI / num_arms);

    // Add a little random wobble so stars form a band around the arm
    // curve rather than sitting exactly on a thin line.
    theta += g_random_double_range(-scatter, scatter);

    // Convert the (radius, theta) polar coordinate into (x, y) Cartesian,
    // same as your existing circular spawn code did.
    out_x = radius * cos(theta);
    out_y = radius * sin(theta);
}

// GTK calls this ~60 times per second
static gint64 last_frame_time = 0; // microseconds
static gboolean on_frame_tick(GtkWidget *widget, GdkFrameClock *clock, gpointer user_data) {
      gint64 now = gdk_frame_clock_get_frame_time(clock); // microseconds
      double delta_time = (last_frame_time == 0) ? 0.0 : (now - last_frame_time) / 1e6;
      last_frame_time = now;

  // 1. Run your C/C++ physics math for all stars
    for (int i = 0; i < NUM_STARS; i++) {
        update_star_physics(stars[i], gravity_constant, delta_time);
    }

    // 2. Tell GTK that the coordinates changed and the canvas needs redrawing
    gtk_widget_queue_draw(widget);

    return G_SOURCE_CONTINUE; // Keep timer running
}

// Activate the window including the drwaring functions!!!
static void activate(GtkApplication *app, gpointer user_data){
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "Galaxy");
  gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
  GtkWidget *drawing_area = gtk_drawing_area_new();
  gtk_window_set_child(GTK_WINDOW(window), drawing_area);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), drawing_stars, NULL, NULL);
  gtk_widget_add_tick_callback(drawing_area, on_frame_tick, NULL, NULL);

  // initialize the starting position and velocities of each start with random values
    double min_radius = 50.0;
    double max_radius = 300.0;

  // init stars position and velocity
  for (int i = 0; i < NUM_STARS; i++) {
      double star_x, star_y;
      if (i < bulge_count) {
          get_bulge_position(20.0, 0.0, min_radius, star_x, star_y);
      } else if (i < bulge_count + halo_count) {
          get_bulge_position(80.0, max_radius, max_radius * 1.3, star_x, star_y);
      } else {
          int arm_index = i % 4;
          get_spiral_position(arm_index, 4, min_radius, max_radius, 0.3, 0.3, star_x, star_y);

      }

    init_star_orbit(stars[i], star_x, star_y, gravity_constant);

  }

  gtk_window_present(GTK_WINDOW(window));
};

int main(int argc, char **argv) {

    // 1. Instantiate the GTK application
    GtkApplication *app = gtk_application_new("my.home.galaxy", G_APPLICATION_DEFAULT_FLAGS);

    // 2. Connect the "activate" signal to your setup function
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    // 3. Hand control over to GTK (blocks here until window closes)
    int status = g_application_run(G_APPLICATION(app), argc, argv);

    // 4. Clean up memory and return exit status
    g_object_unref(app);
    return status;
}


