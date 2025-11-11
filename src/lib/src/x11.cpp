#include "x11.hpp"

#include <dman/display.hpp>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <cmath>
#include <cassert>
#include <cstring>
#include <dman/config.hpp>
#include <stdexcept>

static int x_error_handler(Display *dpy, XErrorEvent *ev)
{
    char buf[128];
    XGetErrorText(dpy, ev->error_code, buf, sizeof(buf));
    std::fprintf(stderr,
                 "X error handler: error_code=%d (%s) request=%d minor=%d "
                 "resource=0x%lx\n",
                 ev->error_code,
                 buf,
                 ev->request_code,
                 ev->minor_code,
                 (unsigned long)ev->resourceid);
    std::fflush(stderr);
    std::abort(); // cause a crash so debugger/backtrace shows the exact call
}

namespace x11
{
session::session()
{
    XSetErrorHandler(x_error_handler);
    display = XOpenDisplay(nullptr);
    if (!display)
        throw std::runtime_error("Failed to open X display.");
    int event_base, error_base;
    if (!XRRQueryExtension(display, &event_base, &error_base))
        throw std::runtime_error(
            "X RandR extension not available on this display.");
    int major, minor;
    XRRQueryVersion(display, &major, &minor);
    primary_output = XRRGetOutputPrimary(display, default_root_window());
}
session::~session()
{
    XCloseDisplay(display);
}

Window session::default_root_window() const
{
    return XDefaultRootWindow(display);
}

screen_resources::screen_resources(session &sess)
{
    contents = XRRGetScreenResources(sess.display, sess.default_root_window());
    if (!contents)
        throw std::runtime_error("Failed to get XRR screen resources.");
}
screen_resources::~screen_resources()
{
    XRRFreeScreenResources(contents);
}
XRRScreenResources *screen_resources::operator->() const
{
    return contents;
}
screen_resources::operator XRRScreenResources *() const
{
    return contents;
}
output_id::output_id(session &sess,
                     screen_resources &resources,
                     uint32_t output_index)
{
    if (output_index >= resources->noutput)
        throw std::out_of_range("Output index out of range.");
    contents = resources->outputs[output_index];
    if (contents == None)
        throw std::runtime_error("Output is None.");
}
output_id::operator RROutput() const
{
    return contents;
}
output_info::output_info(session &sess,
                         screen_resources &resources,
                         const output_id &output)
{
    if ((RROutput)output == None)
        throw std::runtime_error("Output is None.");

    contents = XRRGetOutputInfo(sess.display, resources, output);

    if (!contents)
        throw std::runtime_error("Failed to get XRR output info.");
}
output_info::~output_info()
{
    XRRFreeOutputInfo(contents);
}
XRROutputInfo *output_info::operator->() const
{
    return contents;
}

crtc_info::crtc_info(session &sess, screen_resources &resources, RRCrtc crtc)
{
    contents = XRRGetCrtcInfo(sess.display, resources, crtc);
}
crtc_info::~crtc_info()
{
    XRRFreeCrtcInfo(contents);
}

XRRCrtcInfo *crtc_info::operator->() const
{
    return contents;
}
crtc_info::operator bool() const
{
    return contents != nullptr;
}
device_info::device_info(session &sess)
{
    contents = XListInputDevices(sess.display, &ndevices);
}
device_info::~device_info()
{
    XFreeDeviceList(contents);
}
XDeviceInfo *device_info::operator[](const std::string &name) const
{
    for (int i = 0; i < ndevices; ++i)
    {
        if (name == (std::string)contents[i].name)
        {
            return &contents[i];
        }
    }
    return nullptr;
}
xi_device_info::

    xi_device_info(session &sess, XID device_id)
{
    int ndevices;
    contents = XIQueryDevice(sess.display, device_id, &ndevices);
    if (!contents)
        throw std::runtime_error("Failed to get XI device info.");
}
xi_device_info::~xi_device_info()
{
    XIFreeDeviceInfo(contents);
}
XIDeviceInfo *xi_device_info::operator[](const std::string &name) const
{
    for (int i = 0; i < contents->num_classes; ++i)
    {
        if (name == (std::string)contents[i].name)
        {
            return &contents[i];
        }
    }
    return nullptr;
}
XIDeviceInfo *xi_device_info::operator->() const
{
    return contents;
}
display::vec2<uint32_t> xi_device_info::get_tablet_dimensions() const
{
    display::vec2<uint32_t> result = {0, 0};

    XIAnyClassInfo **classes = contents->classes;

    for (int i = 0; i < contents->num_classes; ++i)
    {
        if (classes[i]->type == XIValuatorClass)
        {
            XIValuatorClassInfo *valuator = (XIValuatorClassInfo *)classes[i];

            if (valuator->number == 0) // X axis
            {
                result.x = valuator->max - valuator->min;
            }
            else if (valuator->number == 1) // Y axis
            {
                result.y = valuator->max - valuator->min;
            }
        }
    }

    return result;
}
x_device::x_device(session &sess, XDeviceInfo *device_info)
{
    contents = XOpenDevice(sess.display, device_info->id);
    if (!contents)
        throw std::runtime_error("Failed to open X device.");
    display = sess.display;
}
x_device::~x_device()
{
    XCloseDevice(display, contents);
}
XDevice *x_device::operator->() const
{
    return contents;
}

bool x_device::set_matrix_prop(const float matrix[3][3])
{

    Atom matrix_prop =
        XInternAtom(display, "Coordinate Transformation Matrix", False);
    if (matrix_prop == None)
        return false;

    const float *float_matrix = &matrix[0][0];

    long long_matrix[9];

    for (int i = 0; i < 9; i++)
    {
        *(float *)(long_matrix + i) = float_matrix[i];
    }

    Atom type;
    int format;
    unsigned long nitems;
    unsigned long bytes_after;
    float *data;
    XGetDeviceProperty(display,
                       contents,
                       matrix_prop,
                       0,
                       9,
                       False,
                       AnyPropertyType,
                       &type,
                       &format,
                       &nitems,
                       &bytes_after,
                       (unsigned char **)&data);

    if (format != 32 || type != XInternAtom(display, "FLOAT", True))

    {
        if (data)
            XFree(data);

        return false;
    }

    XChangeDeviceProperty(display,
                          contents,
                          matrix_prop,
                          type,
                          format,
                          PropModeReplace,
                          (unsigned char *)matrix,
                          9);

    XFree(data);
    XFlush(display);

    return true;
}

RRCrtc find_unused_crtc(x11::session &x11, x11::screen_resources &resources)
{
    for (int i = 0; i < resources->ncrtc; ++i)
    {
        XRRCrtcInfo *crtc_info =
            XRRGetCrtcInfo(x11.display, resources, resources->crtcs[i]);
        if (crtc_info && crtc_info->mode == None)
        {
            XRRFreeCrtcInfo(crtc_info);
            return resources->crtcs[i];
        }
        XRRFreeCrtcInfo(crtc_info);
    }
    throw std::runtime_error("No unused CRTC found.");
}

crtc::crtc(session &_sess, screen_resources &_resources, RRCrtc crtc)
    : sess(_sess), resources(_resources),
      contents(crtc ? crtc : find_unused_crtc(sess, resources))
{
}

crtc::crtc(session &_sess, screen_resources &_resources)
    : sess(_sess), resources(_resources),
      contents(find_unused_crtc(sess, resources))
{
}

crtc::operator RRCrtc() const
{
    return contents;
}

void crtc::set_config(int x,
                      int y,
                      RRMode mode,
                      Rotation rotation,
                      unsigned int output_index,
                      int noutputs)
{
    XRRSetCrtcConfig(sess.display,
                     resources,
                     contents,
                     CurrentTime,
                     x,
                     y,
                     mode,
                     rotation,
                     &resources->outputs[output_index],
                     noutputs);
}

void crtc::clear()
{
    XRRSetCrtcConfig(sess.display,
                     resources,
                     contents,
                     CurrentTime,
                     0,
                     0,
                     None,
                     RR_Rotate_0,
                     nullptr,
                     0);
}

} // namespace x11
