#include <dman/display.hpp>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <cmath>
#include <iostream>
#include <cassert>
#include <cstring>
#include <dman/config.hpp>

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
    RROutput primary_output;

    session()
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
    ~session()
    {
        XCloseDisplay(display);
    }
    session(const session &) = delete;
    session &operator=(const session &) = delete;

    Window default_root_window() const
    {
        return XDefaultRootWindow(display);
    }
};

class screen_resources
{
    XRRScreenResources *contents;

  public:
    screen_resources(session &sess)
    {
        contents =
            XRRGetScreenResources(sess.display, sess.default_root_window());
        if (!contents)
            throw std::runtime_error("Failed to get XRR screen resources.");
    }
    ~screen_resources()
    {
        XRRFreeScreenResources(contents);
    }
    XRRScreenResources *operator->() const
    {
        return contents;
    }
    operator XRRScreenResources *() const
    {
        return contents;
    }
    screen_resources(const screen_resources &) = delete;
    screen_resources &operator=(const screen_resources &) = delete;
};

class output_id
{
    RROutput contents;

  public:
    output_id(session &sess, screen_resources &resources, uint32_t output_index)
    {
        if (output_index >= resources->noutput)
            throw std::out_of_range("Output index out of range.");
        contents = resources->outputs[output_index];
        if (contents == None)
            throw std::runtime_error("Output is None.");
    }
    operator RROutput() const
    {
        return contents;
    }
};

class output_info
{
    XRROutputInfo *contents;

  public:
    output_info(session &sess,
                screen_resources &resources,
                const output_id &output)
    {
        if ((RROutput)output == None)
            throw std::runtime_error("Output is None.");

        contents = XRRGetOutputInfo(sess.display, resources, output);

        if (!contents)
            throw std::runtime_error("Failed to get XRR output info.");
    }
    ~output_info()
    {
        XRRFreeOutputInfo(contents);
    }
    XRROutputInfo *operator->() const
    {
        return contents;
    }
    output_info(const output_info &) = delete;
    output_info &operator=(const output_info &) = delete;
};

class crtc_info
{
    XRRCrtcInfo *contents;

  public:
    crtc_info(session &sess, screen_resources &resources, RRCrtc crtc)
    {
        contents = XRRGetCrtcInfo(sess.display, resources, crtc);
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
    throw std::runtime_error("Target mode not found in mode list.");
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
                   x11::screen_resources &resources,
                   x11::output_info &output_info)
{
    x11::crtc_info crtc_info(x11, resources, output_info->crtc);
    if (!crtc_info)
    {
        std::cerr << "Warning: CRTC info not found for output "
                  << output_info->name << std::endl;
        return false;
    }
    output.is_active = (crtc_info->mode != None);
    XRRModeInfo *mode_info = find_mode_info(resources, crtc_info->mode);
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

size_t get_edid_nitems(x11::session &x11, RROutput output)
{
    Atom edid_atom = XInternAtom(x11.display, "EDID", True);
    if (edid_atom == None)
        return 0;

    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *edid;

    if (Success != XRRGetOutputProperty(x11.display,
                                        output,
                                        edid_atom,
                                        0,
                                        0,
                                        false,
                                        false,
                                        AnyPropertyType,
                                        &actual_type,
                                        &actual_format,
                                        &nitems,
                                        &bytes_after,
                                        &edid))
        return 0;

    if (!edid)
        return 0;

    XFree(edid);

    if (bytes_after % (actual_format / 8) != 0)
        return 0;

    return bytes_after / (actual_format / 8);
}

display::edid get_edid(x11::session &x11, RROutput output)
{
    Atom edid_atom = XInternAtom(x11.display, "EDID", True);
    if (edid_atom == None)
    {
        std::cerr << "Warning: EDID atom not found." << std::endl;
        return {};
    }
    Atom actual_type = 0;
    int actual_format = 0;
    unsigned long nitems = get_edid_nitems(x11, output);
    unsigned long bytes_after = 0;
    unsigned char *edid = nullptr;

    if (!nitems)
    {
        std::cerr << "Warning: No EDID available." << std::endl;
        return {};
    }

    if (Success != XRRGetOutputProperty(x11.display,
                                        output,
                                        edid_atom,
                                        0,
                                        nitems,
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

    int edid_length = nitems * (actual_format / 8);

    display::edid result((void *)edid, edid_length);

    XFree(edid);

    if (result.raw.size() < 128)
    {
        std::cerr << "Warning: EDID data too small (" << result.raw.size()
                  << " bytes)." << std::endl;
        return {};
    }

    return result;
}

// uint32_t count_outputs(x11::session &x11)
// {
//     x11::screen_resources resources(x11);
//     return resources->noutput;
// }

std::vector<display::output> display::get_outputs()
{
    std::vector<display::output> result;

    x11::session x11;
    x11::screen_resources resources(x11);

    for (uint32_t output_index = 0; output_index < resources->noutput;
         output_index++)
    {

        display::output &output = result.emplace_back();
        output.is_primary =
            (resources->outputs[output_index] == x11.primary_output);
        x11::output_id output_id(x11, resources, output_index);
        x11::output_info output_info(x11, resources, output_id);
        output.name = output_info->name;
        if (output_info->connection != RR_Connected)
            continue;

        for (int mode_index = 0; mode_index < output_info->nmode; ++mode_index)
        {
            XRRModeInfo *mode_info =
                find_mode_info(resources, output_info->modes[mode_index]);
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
            if (!set_crtc_info(output, x11, resources, output_info))
            {
                std::cerr << "Warning: Failed to set CRTC info for output "
                          << output_info->name << std::endl;
            }
        }
        output.edid = get_edid(x11, resources->outputs[output_index]);
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

RRMode find_mode_id_by_info(x11::session &x11,
                            x11::screen_resources &resources,
                            const display::mode &target_mode)
{
    for (int i = 0; i < resources->nmode; ++i)
    {
        XRRModeInfo *mode_info = &resources->modes[i];
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

RRMode find_smallest_mode(x11::screen_resources &resources,
                          x11::output_info &output_info)
{
    RRMode smallest_mode = None;
    double smallest_volume = INFINITY;

    for (int mode_index = 0; mode_index < output_info->nmode; ++mode_index)
    {
        XRRModeInfo *mode_info =
            find_mode_info(resources, output_info->modes[mode_index]);
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

bool is_one_display_active(x11::session &x11, x11::screen_resources &resources)
{
    for (size_t i = 0; i < resources->noutput; ++i)
    {
        x11::output_id output_id(x11, resources, i);
        x11::output_info output_info(x11, resources, output_id);
        if (output_info->crtc != None)
            return true;
    }
    return false;
}

void ensure_one_display_is_active(x11::session &x11,
                                  x11::screen_resources &resources)
{

    if (is_one_display_active(x11, resources))
        return;

    std::cerr << "Warning: No active display found. Activating the first "
                 "connected display."
              << std::endl;

    for (size_t i = 0, end = resources->noutput; i < end; i++)
    {
        x11::screen_resources resources(x11);
        x11::output_id output_id(x11, resources, i);
        x11::output_info output_info(x11, resources, output_id);
        if (output_info->connection == RR_Connected && output_info->nmode > 0)
        {
            RRMode mode_id = find_smallest_mode(resources, output_info);
            RRCrtc crtc = find_unused_crtc(x11, resources);
            if (i >= resources->noutput)
                throw std::out_of_range("Output index out of range.");
            RROutput output_id = resources->outputs[i];
            XRRSetCrtcConfig(x11.display,
                             resources,
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

display::vec2<uint32_t> get_total_screen_size(
    const std::unordered_map<std::string, display::state> &outputs)
{
    uint32_t max_x = 0;
    uint32_t max_y = 0;

    for (const auto &[name, state] : outputs)
    {
        if (!state.is_active)
            continue;

        uint32_t right = state.position.x + state.mode.width;
        uint32_t bottom = state.position.y + state.mode.height;

        if (right > max_x)
            max_x = right;
        if (bottom > max_y)
            max_y = bottom;
    }

    return {max_x, max_y};
}

void deactivate_display(x11::session &x11,
                        x11::screen_resources &resources,
                        x11::output_info &output_info)
{
    if (output_info->crtc == None)
        return;

    XRRSetCrtcConfig(x11.display,
                     resources,
                     output_info->crtc,
                     CurrentTime,
                     0,
                     0,
                     None,
                     RR_Rotate_0,
                     nullptr,
                     0);
}

void set_display_config(
    const std::unordered_map<std::string, display::state> &outputs,
    x11::session &x11,
    x11::screen_resources &resources)
{
    for (uint32_t output_index = 0, end = resources->noutput;
         output_index < end;
         output_index++)
    {
        x11::output_id output_id(x11, resources, output_index);
        x11::output_info output_info(x11, resources, output_id);

        if (output_info->connection != RR_Connected)
        {
            deactivate_display(x11, resources, output_info);
            continue;
        }

        display::edid edid = get_edid(x11, output_id);

        const auto &it = outputs.find(edid.digest.hex());

        if (it == outputs.end())
        {
            deactivate_display(x11, resources, output_info);
            continue;
        }

        const display::state &want = it->second;

        if (!want.is_active)
        {
            deactivate_display(x11, resources, output_info);
            continue;
        }

        RRMode mode_id = find_mode_id_by_info(x11, resources, want.mode);
        Rotation rotation = rotation_to_x11_rotation(want.rotation);
        RRCrtc crtc = output_info->crtc ? output_info->crtc
                                        : find_unused_crtc(x11, resources);

        RROutput id_copy = (RROutput)output_id;
        XRRSetCrtcConfig(x11.display,
                         (XRRScreenResources *)resources,
                         crtc,
                         CurrentTime,
                         want.position.x,
                         want.position.y,
                         mode_id,
                         rotation,
                         &id_copy,
                         1);

        if (want.is_primary)
        {
            XRRSetOutputPrimary(x11.display,
                                x11.default_root_window(),
                                output_id);
        }
    }

    static constexpr size_t pixels_per_milimeter = 3;
    display::vec2<uint32_t> total_size = get_total_screen_size(outputs);

    if (total_size.x > 0 && total_size.y > 0)
    {
        XRRSetScreenSize(x11.display,
                         x11.default_root_window(),
                         total_size.x,
                         total_size.y,
                         total_size.x / pixels_per_milimeter,
                         total_size.y / pixels_per_milimeter);
    }
    else
    {
        std::cerr << "Warning: Total screen size is zero; not setting screen "
                     "size."
                  << std::endl;
    }
}

void display::set_outputs(
    const std::unordered_map<std::string, display::state> &outputs)
{
    x11::session x11;
    x11::screen_resources resources(x11);
    set_display_config(outputs, x11, resources);
    ensure_one_display_is_active(x11, resources);
}

display::edid::edid(const void *begin, size_t size)
{
    if (size > 0)
    {
        raw.resize(size);
        std::memcpy(raw.data(), begin, size);
        digest = digest::sha256(begin, size);
    }

    uint16_t manufacturer_id_raw_little_endian =
        (static_cast<uint16_t>(raw[8]) << 8) | raw[9];

    uint8_t manufacturer_id_five_bit_offsets[3] = {
        static_cast<uint8_t>((manufacturer_id_raw_little_endian >> 10) & 0x1F),
        static_cast<uint8_t>((manufacturer_id_raw_little_endian >> 5) & 0x1F),
        static_cast<uint8_t>(manufacturer_id_raw_little_endian & 0x1F),
    };

    manufacturer_id = std::string({
        static_cast<char>(manufacturer_id_five_bit_offsets[0] + 'A' - 1),
        static_cast<char>(manufacturer_id_five_bit_offsets[1] + 'A' - 1),
        static_cast<char>(manufacturer_id_five_bit_offsets[2] + 'A' - 1),
    });

    uint16_t manufacturer_product_code_i = *(uint16_t *)&raw[10];

    char manufacturer_product_code_hex[5];
    std::snprintf(manufacturer_product_code_hex,
                  sizeof(manufacturer_product_code_hex),
                  "%04X",
                  manufacturer_product_code_i);

    manufacturer_product_code = std::string(manufacturer_product_code_hex);

    uint32_t serial_number_i = *(uint32_t *)&raw[12];

    char serial_number_hex[9];
    std::snprintf(serial_number_hex,
                  sizeof(serial_number_hex),
                  "%08X",
                  serial_number_i);

    serial_number = std::string(serial_number_hex);

    name =
        manufacturer_id + "-" + manufacturer_product_code + "-" + serial_number;
}

void display::output::operator=(const mode &mode)
{
    mode_index = get_mode_index(modes, mode);
}

display::output::operator display::state() const
{
    return display::state{
        .mode = modes[mode_index],
        .position = position,
        .rotation = rotation,
        .is_primary = is_primary,
        .is_active = is_active,
    };
}

void display::output::operator=(const state &state)
{
    is_active = state.is_active;

    if (!is_active)
        return;

    if (!modes.empty())
        *this = state.mode;
    position = state.position;
    rotation = state.rotation;
    is_primary = state.is_primary;
}