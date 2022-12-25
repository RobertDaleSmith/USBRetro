/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb.h"

uint16_t buttons;

extern void __not_in_flash_func(post_globals)(uint8_t dev_addr, uint16_t buttons, uint8_t delta_x, uint8_t delta_y);

//--------------------------------------------------------------------+
// USB X-input
//--------------------------------------------------------------------+
#if CFG_TUH_XINPUT
void tuh_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len)
{
  xinputh_interface_t *xid_itf = (xinputh_interface_t *)report;
  xinput_gamepad_t *p = &xid_itf->pad;
  const char* type_str;
  switch (xid_itf->type)
  {
    case 1: type_str = "Xbox One";          break;
    case 2: type_str = "Xbox 360 Wireless"; break;
    case 3: type_str = "Xbox 360 Wired";    break;
    case 4: type_str = "Xbox OG";           break;
    default: type_str = "Unknown";
  }

  if (xid_itf->connected && xid_itf->new_pad_data)
  {
    printf("[%02x, %02x], Type: %s, Buttons %04x, LT: %02x RT: %02x, LX: %d, LY: %d, RX: %d, RY: %d\n",
      dev_addr, instance, type_str, p->wButtons, p->bLeftTrigger, p->bRightTrigger, p->sThumbLX, p->sThumbLY, p->sThumbRX, p->sThumbRY);

              //  ((p->wButtons & XINPUT_GAMEPAD_DPAD_DOWN || p->sThumbLY < -20000) ? 0x00 : 0x04) |
              //  ((p->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT || p->sThumbLX > 20000) ? 0x00 : 0x02) |

    buttons = (((p->wButtons & XINPUT_GAMEPAD_B)     ? 0x8000 : 0x00) | //C-DOWN
               ((p->wButtons & XINPUT_GAMEPAD_A)     ? 0x4000 : 0x00) | //A
               ((p->wButtons & XINPUT_GAMEPAD_START) ? 0x2000 : 0x00) | //START
               ((p->wButtons & XINPUT_GAMEPAD_BACK)  ? 0x1000 : 0x00) | //NUON
               ((p->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  ? 0x0800 : 0x00) | //D-DOWN
               ((p->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  ? 0x0400 : 0x00) | //D-LEFT
               ((p->wButtons & XINPUT_GAMEPAD_DPAD_UP)    ? 0x0200 : 0x00) | //D-UP
               ((p->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ? 0x0100 : 0x00) | //D-RIGHT
               ((1)                ? 0x0080 : 0x00) |
               ((0)                ? 0x0040 : 0x00) |
               ((p->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)  ? 0x0020 : 0x00) | //L
               ((p->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 0x0010 : 0x00) | //R
               ((p->wButtons & XINPUT_GAMEPAD_X) ? 0x0008 : 0x00) | //B
               ((p->wButtons & XINPUT_GAMEPAD_Y) ? 0x0004 : 0x00) | //C-LEFT
               ((p->bLeftTrigger)  ? 0x0002 : 0x00) | //C-UP
               ((p->bRightTrigger) ? 0x0001 : 0x00)); //C-RIGHT

    post_globals(dev_addr, buttons, p->sThumbLX, p->sThumbLY);
  }
  tuh_xinput_receive_report(dev_addr, instance);
}

void tuh_xinput_mount_cb(uint8_t dev_addr, uint8_t instance, const xinputh_interface_t *xinput_itf)
{
  printf("XINPUT MOUNTED %02x %d\n", dev_addr, instance);
  // If this is a Xbox 360 Wireless controller we need to wait for a connection packet
  // on the in pipe before setting LEDs etc. So just start getting data until a controller is connected.
  if (xinput_itf->type == XBOX360_WIRELESS && xinput_itf->connected == false)
  {
    tuh_xinput_receive_report(dev_addr, instance);
    return;
  }
  tuh_xinput_set_led(dev_addr, instance, 0, true);
  tuh_xinput_set_led(dev_addr, instance, 1, true);
  tuh_xinput_set_rumble(dev_addr, instance, 0, 0, true);
  tuh_xinput_receive_report(dev_addr, instance);
}

void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  printf("XINPUT UNMOUNTED %02x %d\n", dev_addr, instance);
}

#endif
