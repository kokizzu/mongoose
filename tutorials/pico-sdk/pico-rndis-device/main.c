// Copyright (c) 2022 Cesanta Software Limited
// All rights reserved

#include "mongoose.h"
#include "net.h"
#include "pico/stdlib.h"
#include "tusb.h"

char *g_firmware_version = "1.0.0";

bool hal_gpio_read(int pin) {
  return (pin >= 0 && pin <= 29) ? gpio_get_out_level((uint) pin) : false;
}

bool hal_gpio_write(int pin, bool val) {
  if (pin >= 0 && pin <= 29) {
    gpio_put((uint) pin, val);
    return true;
  } else {
    return false;
  }
}

int hal_led_pin(void) {
  return PICO_DEFAULT_LED_PIN;
}

static struct mg_tcpip_if *s_ifp;

uint8_t tud_network_mac_address[6] = {2, 2, 0x84, 0x6A, 0x96, 0};

bool tud_network_recv_cb(const uint8_t *buf, uint16_t len) {
  mg_tcpip_qwrite((void *) buf, len, s_ifp);
  // MG_INFO(("RECV %hu", len));
  // mg_hexdump(buf, len);
  tud_network_recv_renew();
  return true;
}

void tud_network_init_cb(void) {
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
  // MG_INFO(("SEND %hu", arg));
  memcpy(dst, ref, arg);
  return arg;
}

static size_t usb_tx(const void *buf, size_t len, struct mg_tcpip_if *ifp) {
  if (!tud_ready()) return 0;
  while (!tud_network_can_xmit(len)) tud_task();
  tud_network_xmit((void *) buf, len);
  (void) ifp;
  return len;
}

static bool usb_poll(struct mg_tcpip_if *ifp, bool s1) {
  (void) ifp;
  tud_task();
  return s1 ? tud_inited() && tud_ready() && tud_connected() : false;
}

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_dta) {
  if (ev == MG_EV_HTTP_MSG) return mg_http_reply(c, 200, "", "ok\n");
}

int main(void) {
  gpio_init(PICO_DEFAULT_LED_PIN);
  gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
  stdio_init_all();

  struct mg_mgr mgr;  // Initialise Mongoose event manager
  mg_mgr_init(&mgr);  // and attach it to the interface

  struct mg_tcpip_driver driver = {.tx = usb_tx, .poll = usb_poll};
  struct mg_tcpip_if mif = {.mac = {2, 0, 1, 2, 3, 0x77},
                            .ip = mg_htonl(MG_U32(192, 168, 3, 1)),
                            .mask = mg_htonl(MG_U32(255, 255, 255, 0)),
                            .enable_dhcp_server = true,
                            .enable_get_gateway = true,
                            .driver = &driver,
                            .recv_queue.size = 4096};
  s_ifp = &mif;
  mg_tcpip_init(&mgr, &mif);
  tusb_init();

  MG_INFO(("Initialising application..."));
  web_init(&mgr);

  MG_INFO(("Starting event loop"));
  for (;;) {
    mg_mgr_poll(&mgr, 0);
  }

  return 0;
}
