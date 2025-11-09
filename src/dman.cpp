#include "dman.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <cmath>
#include <iostream>
#include <cassert>
#include <cstring>

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
class session
{
  public:
    Display *display;
    Window root;
    XRRScreenResources *resources;
    RROutput primary_output;

    session()
    {
        display = XOpenDisplay(nullptr);
        root = DefaultRootWindow(display);
        resources = XRRGetScreenResources(display, root);
        primary_output = XRRGetOutputPrimary(display, root);
        XSetErrorHandler(x_error_handler);
    }
    ~session()
    {
        XRRFreeScreenResources(resources);
        XCloseDisplay(display);
    }
};

class output_info
{
    XRROutputInfo *contents;

  public:
    output_info(session &sess, uint32_t output_index)
    {
        contents = XRRGetOutputInfo(sess.display,
                                    sess.resources,
                                    sess.resources->outputs[output_index]);
    }
    ~output_info()
    {
        XRRFreeOutputInfo(contents);
    }
    XRROutputInfo *operator->() const
    {
        return contents;
    }
};

class crtc_info
{
    XRRCrtcInfo *contents;

  public:
    crtc_info(session &sess, RRCrtc crtc)
    {
        contents = XRRGetCrtcInfo(sess.display, sess.resources, crtc);
    }
    ~crtc_info()
    {
        XRRFreeCrtcInfo(contents);
    }
    XRRCrtcInfo *operator->() const
    {
        return contents;
    }
    operator bool() const
    {
        return contents != nullptr;
    }
};

} // namespace x11

XRRModeInfo *find_mode_info(XRRScreenResources *resources, RRMode mode_id)
{
    for (int i = 0; i < resources->nmode; ++i)
    {
        if (resources->modes[i].id == mode_id)
        {
            return &resources->modes[i];
        }
    }
    return nullptr;
}

bool display::mode::operator==(const display::mode &other) const
{
    return (width == other.width) && (height == other.height) &&
           std::fabs(rate - other.rate) < 1.5;
}

uint32_t get_mode_index(const std::vector<display::mode> &modes,
                        const display::mode &target_mode)
{
    for (size_t i = 0, size = modes.size(); i < size; i++)
    {
        if (modes[i] == target_mode)
        {
            return static_cast<uint32_t>(i);
        }
    }
    std::cerr << "Warning: Target mode not found in mode list." << std::endl;
    return 0;
}

display::mode calc_mode_from_info(XRRModeInfo *info)
{
    display::mode result = {.name = info->name ? info->name : ""};

    result.width = info->width;
    result.height = info->height;
    result.rate = (float)info->dotClock / (info->hTotal * info->vTotal);

    return result;
}

bool set_crtc_info(display::output &output,
                   x11::session &x11,
                   x11::output_info &output_info)
{
    x11::crtc_info crtc_info(x11, output_info->crtc);
    if (!crtc_info)
    {
        std::cerr << "Warning: CRTC info not found for output "
                  << output_info->name << std::endl;
        return false;
    }
    output.is_active = (crtc_info->mode != None);
    XRRModeInfo *mode_info = find_mode_info(x11.resources, crtc_info->mode);
    if (!mode_info)
    {
        std::cerr << "Warning: Mode ID " << crtc_info->mode
                  << " not found in resources." << std::endl;
        return false;
    }
    output.mode_index =
        get_mode_index(output.modes, calc_mode_from_info(mode_info));
    output.position.x = crtc_info->x;
    output.position.y = crtc_info->y;
    switch (crtc_info->rotation)
    {
    case RR_Rotate_0:
        output.rotation = display::rotation::NORMAL;
        break;
    case RR_Rotate_90:
        output.rotation = display::rotation::RIGHT;
        break;
    case RR_Rotate_180:
        output.rotation = display::rotation::INVERTED;
        break;
    case RR_Rotate_270:
        output.rotation = display::rotation::LEFT;
        break;
    default:
        std::cerr << "Warning: Unknown rotation value " << crtc_info->rotation
                  << " for output " << output_info->name << std::endl;
        output.rotation = display::rotation::NORMAL;
        break;
    }

    return true;
}

constexpr size_t EDID_v1_4_SIZE = 128;

display::edid get_edid(x11::session &x11, RROutput output)
{
    Atom edid_atom = XInternAtom(x11.display, "EDID", True);
    if (edid_atom == None)
    {
        std::cerr << "Warning: EDID atom not found." << std::endl;
        return {};
    }
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *edid;
    if (Success != XRRGetOutputProperty(x11.display,
                                        output,
                                        edid_atom,
                                        0,
                                        EDID_v1_4_SIZE,
                                        false,
                                        false,
                                        AnyPropertyType,
                                        &actual_type,
                                        &actual_format,
                                        &nitems,
                                        &bytes_after,
                                        &edid))
    {
        std::cerr << "Warning: Failed to get EDID property." << std::endl;
        return {};
    }
    display::edid result((void *)edid, EDID_v1_4_SIZE);

    XFree(edid);

    return result;
}

display::session::operator std::vector<display::output>()
{
    std::vector<display::output> result;

    x11::session x11;

    for (uint32_t output_index = 0; output_index < x11.resources->noutput;
         ++output_index)
    {
        display::output &output = result.emplace_back();
        output.is_primary =
            (x11.resources->outputs[output_index] == x11.primary_output);
        x11::output_info output_info(x11, output_index);
        output.name = output_info->name;
        if (output_info->connection != RR_Connected)
            continue;

        for (int mode_index = 0; mode_index < output_info->nmode; ++mode_index)
        {
            XRRModeInfo *mode_info =
                find_mode_info(x11.resources, output_info->modes[mode_index]);
            if (!mode_info)
            {
                std::cerr << "Warning: Mode ID "
                          << output_info->modes[mode_index]
                          << " not found in resources." << std::endl;
                continue;
            }

            output.modes.emplace_back(calc_mode_from_info(mode_info));
        }

        if (output_info->crtc)
        {
            if (!set_crtc_info(output, x11, output_info))
            {
                std::cerr << "Warning: Failed to set CRTC info for output "
                          << output_info->name << std::endl;
            }
        }
        output.edid = get_edid(x11, x11.resources->outputs[output_index]);
    }

    return result;
}

const display::output *
find_output_by_name(const std::vector<display::output> &outputs,
                    const std::string &name)
{
    for (auto &output : outputs)
    {
        if (output.name == name)
        {
            return &output;
        }
    }
    return nullptr;
}

RRMode find_mode_id_by_info(x11::session &x11, const display::mode &target_mode)
{
    for (int i = 0; i < x11.resources->nmode; ++i)
    {
        XRRModeInfo *mode_info = &x11.resources->modes[i];
        display::mode mode = calc_mode_from_info(mode_info);
        if (mode == target_mode)
        {
            return mode_info->id;
        }
    }
    throw std::runtime_error("Mode not found in resources.");
}

Rotation rotation_to_x11_rotation(display::rotation rotation)
{
    switch (rotation)
    {
    case display::rotation::NORMAL:
        return RR_Rotate_0;
    case display::rotation::LEFT:
        return RR_Rotate_270;
    case display::rotation::RIGHT:
        return RR_Rotate_90;
    case display::rotation::INVERTED:
        return RR_Rotate_180;
    default:
        return RR_Rotate_0;
    }
}

RRCrtc find_unused_crtc(x11::session &x11)
{
    for (int i = 0; i < x11.resources->ncrtc; ++i)
    {
        XRRCrtcInfo *crtc_info =
            XRRGetCrtcInfo(x11.display, x11.resources, x11.resources->crtcs[i]);
        if (crtc_info && crtc_info->mode == None)
        {
            XRRFreeCrtcInfo(crtc_info);
            return x11.resources->crtcs[i];
        }
        XRRFreeCrtcInfo(crtc_info);
    }
    throw std::runtime_error("No unused CRTC found.");
}

RRMode find_smallest_mode(x11::session &x11, x11::output_info &output_info)
{
    RRMode smallest_mode = None;
    double smallest_volume = INFINITY;

    for (int mode_index = 0; mode_index < output_info->nmode; ++mode_index)
    {
        XRRModeInfo *mode_info =
            find_mode_info(x11.resources, output_info->modes[mode_index]);
        if (!mode_info)
        {
            continue;
        }
        display::mode mode = calc_mode_from_info(mode_info);
        double volume = (double)mode.width * (double)mode.height * mode.rate;
        if (volume < smallest_volume)
        {
            smallest_volume = volume;
            smallest_mode = mode_info->id;
        }
    }
    return smallest_mode;
}

void ensure_one_display_is_active(x11::session &x11)
{
    for (size_t i = 0; i < x11.resources->noutput; ++i)
    {
        x11::output_info output_info(x11, i);
        if (output_info->crtc != None)
            return;
    }

    std::cerr << "Warning: No active display found. Activating the first "
                 "connected display."
              << std::endl;

    for (size_t i = 0; i < x11.resources->noutput; ++i)
    {
        x11::output_info output_info(x11, i);
        if (output_info->connection == RR_Connected && output_info->nmode > 0)
        {
            RRMode mode_id = find_smallest_mode(x11, output_info);
            RRCrtc crtc = find_unused_crtc(x11);
            RROutput output_id = x11.resources->outputs[i];
            XRRSetCrtcConfig(x11.display,
                             x11.resources,
                             crtc,
                             CurrentTime,
                             0,
                             0,
                             mode_id,
                             RR_Rotate_0,
                             &output_id,
                             1);
            return;
        }
    }
}

display::vec2<uint32_t>
get_total_screen_size(const std::vector<display::output> &outputs)
{
    uint32_t max_x = 0;
    uint32_t max_y = 0;

    for (const auto &output : outputs)
    {
        if (!output.is_active)
            continue;

        const display::mode &mode = output.modes[output.mode_index];
        uint32_t right = output.position.x + mode.width;
        uint32_t bottom = output.position.y + mode.height;

        if (right > max_x)
            max_x = right;
        if (bottom > max_y)
            max_y = bottom;
    }

    return {max_x, max_y};
}

void display::session::operator=(const std::vector<display::output> &outputs)
{
    x11::session x11;

    for (uint32_t output_index = 0; output_index < x11.resources->noutput;
         ++output_index)
    {
        x11::output_info output_info(x11, output_index);
        RROutput output_id = x11.resources->outputs[output_index];

        const display::output *target_output =
            find_output_by_name(outputs, output_info->name);

        if (!target_output || !target_output->is_active)
        {
            if (output_info->crtc == None)
                continue;

            XRRSetCrtcConfig(x11.display,
                             x11.resources,
                             output_info->crtc,
                             CurrentTime,
                             0,
                             0,
                             None,
                             RR_Rotate_0,
                             nullptr,
                             0);
            continue;
        }

        const display::mode &target_mode =
            target_output->modes[target_output->mode_index];

        RRMode mode_id = find_mode_id_by_info(x11, target_mode);
        Rotation rotation = rotation_to_x11_rotation(target_output->rotation);
        RRCrtc crtc =
            output_info->crtc ? output_info->crtc : find_unused_crtc(x11);
        XRRSetCrtcConfig(x11.display,
                         x11.resources,
                         crtc,
                         CurrentTime,
                         target_output->position.x,
                         target_output->position.y,
                         mode_id,
                         rotation,
                         &output_id,
                         1);

        if (target_output->is_primary)
        {
            XRRSetOutputPrimary(x11.display, x11.root, output_id);
        }
    }

    static constexpr size_t pixels_per_milimeter = 3;
    display::vec2<uint32_t> total_size = get_total_screen_size(outputs);

    XRRSetScreenSize(x11.display,
                     x11.root,
                     total_size.x,
                     total_size.y,
                     total_size.x / pixels_per_milimeter,
                     total_size.y / pixels_per_milimeter);

    ensure_one_display_is_active(x11);
}

display::edid::edid(const void *data, size_t size)
{
    if (size > 0)
    {
        raw.resize(size);
        std::memcpy(raw.data(), data, size);
        digest = digest::sha256(data, size);
    }
}