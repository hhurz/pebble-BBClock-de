/*

BB analog watch

 */

#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#define MY_UUID {0xE3, 0xF4, 0x45, 0xA2, 0x0F, 0x77, 0x41, 0x43, 0x8D, 0x0F, 0xD6, 0x12, 0xA8, 0xC0, 0x2E, 0xB3}
PBL_APP_INFO(MY_UUID, "BB Analog", "hhurz", 0x1, 0x0, RESOURCE_ID_IMAGE_MENU_ICON, APP_INFO_WATCH_FACE);


Window window;

BmpContainer background_image_container;
TextLayer text_ampm_layer;
TextLayer text_day_layer;
RotBmpContainer hour_hand_image_container;
RotBmpContainer minute_hand_image_container;

#define MAX(a,b) (((a)>(b))?(a):(b))

#define SQRT_MAGIC_F 0x5f3759df
float my_sqrt(const float x)
{
  const float xhalf = 0.5f*x;

  union // get bits for floating value
  {
    float x;
    int i;
  } u;
  u.x = x;
  u.i = SQRT_MAGIC_F - (u.i >> 1);  // gives initial guess y0
  return x*u.x*(1.5f - xhalf*u.x*u.x);// Newton step, repeating increases accuracy
}


void rot_bitmap_set_src_ic(RotBitmapLayer *image, GPoint ic)
{
  image->src_ic = ic;

  // adjust the frame so the whole image will still be visible
  const int32_t horiz = MAX(ic.x, abs(image->bitmap->bounds.size.w - ic.x));
  const int32_t vert = MAX(ic.y, abs(image->bitmap->bounds.size.h - ic.y));

  GRect r = layer_get_frame(&image->layer);

  // Fudge to deal with non-even dimensions--to ensure right-most and bottom-most edges aren't cut off.
  const int32_t new_dist = (int32_t) (my_sqrt(horiz*horiz + vert*vert) * 2) + 1;

  r.size.w = new_dist;
  r.size.h = new_dist;
  layer_set_frame(&image->layer, r);

  r.origin = GPoint(0, 0);
  image->layer.bounds = r;

  image->dest_ic = GPoint(new_dist / 2, new_dist / 2);

  layer_mark_dirty(&(image->layer));
}


void set_hand_angle(RotBmpContainer *hand_image_container, unsigned int hand_angle)
{
  signed short x_fudge = 0;
  signed short y_fudge = 0;

  hand_image_container->layer.rotation =  TRIG_MAX_ANGLE * hand_angle / 360;

  //
  // Due to rounding/centre of rotation point/other issues of fitting
  // square pixels into round holes by the time hands get to 6 and 9
  // o'clock there's off-by-one pixel errors.
  //
  // The `x_fudge` and `y_fudge` values enable us to ensure the hands
  // look centred on the minute marks at those points. (This could
  // probably be improved for intermediate marks also but they're not
  // as noticable.)
  //
  // I think ideally we'd only ever calculate the rotation between
  // 0-90 degrees and then rotate again by 90 or 180 degrees to
  // eliminate the error.
  //
  if (hand_angle == 180)
    x_fudge = -1;
  else if (hand_angle == 270)
    y_fudge = -1;

  // (144 = screen width, 168 = screen height)
  hand_image_container->layer.layer.frame.origin.x = (144/2) - (hand_image_container->layer.layer.frame.size.w/2) + x_fudge;
  hand_image_container->layer.layer.frame.origin.y = (168/2) - (hand_image_container->layer.layer.frame.size.h/2) + y_fudge;

  layer_mark_dirty(&hand_image_container->layer.layer);
}


void update_display(PblTm *tick_time)
{
  static char day_text[16], ampm_text[8];
  static char szOld[] = "                  ";
  static char szNew[] = "                  ";
  char szDay[8], szAMPM [8];
  int w = tick_time->tm_wday;

  strcpy ( szDay, (w == 0 ? "So" : w == 1 ? "Mo" : w == 2 ? "Di" : w == 3 ? "Mi" : w == 4 ? "Do" : w == 5 ? "Fr" : "Sa") );
  string_format_time ( szAMPM, sizeof(szAMPM), "%p", tick_time );

  strcpy ( szNew, szAMPM );
  strcat ( szNew, szDay );

  if ( strcmp(szNew, szOld) != 0 )
  {
    strcpy ( ampm_text, szAMPM );
    strcpy ( day_text, szDay );
    string_format_time ( szDay, sizeof(szDay), "%e", tick_time );
    if ( *szDay != ' ' )
      strcat ( day_text, " " );
    strcat ( day_text, szDay );

    text_layer_set_text ( &text_ampm_layer, ampm_text );
    text_layer_set_text ( &text_day_layer, day_text );

    strcpy ( szOld, szNew );
  }

  set_hand_angle(&hour_hand_image_container, ((tick_time->tm_hour % 12) * 30) + (tick_time->tm_min/2));
  set_hand_angle(&minute_hand_image_container, tick_time->tm_min * 6);
}


void text_layer_setup (Window *window, TextLayer *layer, int x, int y, int w, int h, int FontId, GTextAlignment Align)
{
  text_layer_init ( layer, GRect(x,y,w,h) );
  text_layer_set_text_color ( layer, GColorWhite );
  text_layer_set_background_color ( layer, GColorClear );
  text_layer_set_font ( layer, fonts_load_custom_font(resource_get_handle(FontId)) );
  text_layer_set_text_alignment ( layer, Align );
  layer_add_child ( &window->layer, &layer->layer );
}


void handle_init(AppContextRef ctx)
{
  (void)ctx;

  window_init(&window, "BB Analog");
  window_stack_push(&window, true);

  resource_init_current_app(&APP_RESOURCES);

  // Set up a layer for the static watch face background
  bmp_init_container(RESOURCE_ID_IMAGE_BACKGROUND, &background_image_container);
  layer_add_child(&window.layer, &background_image_container.layer.layer);

  // Set up a layer for the hour hand
  rotbmp_init_container(RESOURCE_ID_IMAGE_HOUR_HAND, &hour_hand_image_container);
  rot_bitmap_set_src_ic(&hour_hand_image_container.layer, GPoint(4, 37));
  layer_add_child(&window.layer, &hour_hand_image_container.layer.layer);

  // Set up a layer for the minute hand
  rotbmp_init_container(RESOURCE_ID_IMAGE_MINUTE_HAND, &minute_hand_image_container);
  rot_bitmap_set_src_ic(&minute_hand_image_container.layer, GPoint(2, 56));
  layer_add_child(&window.layer, &minute_hand_image_container.layer.layer);

  text_layer_setup ( &window, &text_ampm_layer,
                     24, 168/2-11, 144/2-24-18, 22,
                     RESOURCE_ID_FONT_ROBOTO_CONDENSED_16,
                     GTextAlignmentLeft );

  text_layer_setup ( &window, &text_day_layer,
                     144/2+18, 168/2-11, 144/2-18-16, 22,
                     RESOURCE_ID_FONT_ROBOTO_CONDENSED_16,
                     GTextAlignmentLeft );

  PblTm tick_time;
  get_time(&tick_time);
  update_display(&tick_time);
}

void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t)
{
  (void)ctx;
  PblTm tick_time;
  memcpy ( &tick_time, t->tick_time, sizeof(PblTm) );
  update_display(&tick_time);
}

void handle_deinit(AppContextRef ctx)
{
  (void)ctx;

  bmp_deinit_container(&background_image_container);
  rotbmp_deinit_container(&hour_hand_image_container);
  rotbmp_deinit_container(&minute_hand_image_container);
}

void pbl_main(void *params)
{
  PebbleAppHandlers handlers =
  {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
    .tick_info = {
      .tick_handler = &handle_minute_tick,
      .tick_units = MINUTE_UNIT
    }

  };
  app_event_loop(params, &handlers);
}
