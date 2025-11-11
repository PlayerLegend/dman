#pragma once

#include <dman/display.hpp>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <cstdint>
#include <string>

namespace x11
{

class session
{
  public:
    Display *display;
    RROutput primary_output;

    session();
    ~session();

    session(const session &) = delete;
    session &operator=(const session &) = delete;

    Window default_root_window() const;
};

class screen_resources
{
    XRRScreenResources *contents;

  public:
    explicit screen_resources(session &sess);
    ~screen_resources();

    XRRScreenResources *operator->() const;
    operator XRRScreenResources *() const;

    screen_resources(const screen_resources &) = delete;
    screen_resources &operator=(const screen_resources &) = delete;

    XRRModeInfo *find_mode_info(RRMode mode_id) const;
};

class output_id
{
    RROutput contents;

  public:
    output_id(session &sess,
              screen_resources &resources,
              uint32_t output_index);
    operator RROutput() const;
};

class output_info
{
    XRROutputInfo *contents;

  public:
    output_info(session &sess,
                screen_resources &resources,
                const output_id &output);
    ~output_info();

    XRROutputInfo *operator->() const;

    output_info(const output_info &) = delete;
    output_info &operator=(const output_info &) = delete;
};

class crtc_info
{
    XRRCrtcInfo *contents;

  public:
    crtc_info(session &sess, screen_resources &resources, RRCrtc crtc);
    ~crtc_info();

    XRRCrtcInfo *operator->() const;
    operator bool() const;
};

class device_info
{
    XDeviceInfo *contents;
    int ndevices;

  public:
    explicit device_info(session &sess);
    ~device_info();

    XDeviceInfo *operator[](const std::string &name) const;
};

class xi_device_info
{
    XIDeviceInfo *contents;

  public:
    xi_device_info(session &sess, XID device_id);
    ~xi_device_info();

    XIDeviceInfo *operator[](const std::string &name) const;
    XIDeviceInfo *operator->() const;

    display::vec2<uint32_t> get_tablet_dimensions() const;
};

class x_device
{
    XDevice *contents;
    Display *display;

  public:
    x_device(session &sess, XDeviceInfo *device_info);
    ~x_device();

    XDevice *operator->() const;

    bool set_coodinate_transformation_matrix(const float matrix[3][3]);
};

class crtc
{
    session &sess;
    screen_resources &resources;
    RRCrtc contents;

  public:
    crtc(session &sess, screen_resources &resources, RRCrtc crtc);
    crtc(session &sess, screen_resources &resources);
    operator RRCrtc() const;
    void set_config(int x,
                    int y,
                    RRMode mode,
                    Rotation rotation,
                    unsigned int output_index,
                    int noutputs);
    void clear();
};

} // namespace x11