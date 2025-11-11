#include <dman/display.hpp>
#include <cmath>
#include <iostream>
#include <cassert>
#include <cstring>
#include <dman/config.hpp>

#include "x11.hpp"

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
    XRRModeInfo *mode_info = resources.find_mode_info(crtc_info->mode);
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

display::output init_output(x11::session &x11,
                            x11::screen_resources &resources,
                            uint32_t output_index)
{
    display::output output;

    x11::output_id output_id(x11, resources, output_index);
    x11::output_info output_info(x11, resources, output_id);
    output.name = output_info->name;
    output.is_primary =
        (resources->outputs[output_index] == x11.primary_output);
    if (output_info->connection != RR_Connected)
        return output;

    for (int mode_index = 0; mode_index < output_info->nmode; ++mode_index)
    {
        XRRModeInfo *mode_info =
            resources.find_mode_info(output_info->modes[mode_index]);

        if (!mode_info)
        {
            std::cerr << "Warning: Mode ID " << output_info->modes[mode_index]
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

    return output;
}

std::vector<display::output> display::get_outputs()
{
    std::vector<display::output> result;

    x11::session x11;
    x11::screen_resources resources(x11);

    for (uint32_t output_index = 0; output_index < resources->noutput;
         output_index++)
        result.emplace_back(init_output(x11, resources, output_index));

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

RRMode find_smallest_mode(x11::screen_resources &resources,
                          x11::output_info &output_info)
{
    RRMode smallest_mode = None;
    double smallest_volume = INFINITY;

    for (int mode_index = 0; mode_index < output_info->nmode; ++mode_index)
    {
        XRRModeInfo *mode_info =
            resources.find_mode_info(output_info->modes[mode_index]);
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
            x11::crtc crtc(x11, resources);
            crtc.set_config(0, 0, mode_id, RR_Rotate_0, i, 1);
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

    x11::crtc crtc(x11, resources, output_info->crtc);
    crtc.clear();
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
        x11::crtc crtc(x11, resources, output_info->crtc);

        crtc.set_config(want.position.x,
                        want.position.y,
                        mode_id,
                        rotation,
                        output_index,
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

void multiply_matrices(float result[3][3], float a[3][3], float b[3][3])
{
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            result[i][j] = 0;
            for (int k = 0; k < 3; k++)
            {
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }
}

void generate_transform_matrix(display::state state,
                               float transform_matrix[3][3])
{
    // Initialize identity matrix
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            transform_matrix[i][j] = (i == j) ? 1.0 : 0.0;
        }
    }

    // Translate matrix
    float translate[3][3] = {
        {1, 0, (float)state.position.x},
        {0, 1, (float)state.position.y},
        {0, 0, 1},
    };

    // Rotation matrix
    float rotation[3][3] = {
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1},
    };

    float scale[3][3] = {
        {(float)state.mode.width, 0, 0},
        {0, (float)state.mode.height, 0},
        {0, 0, 1},
    };

#ifndef M_PI
#define M_PI 3.14159
#endif

    if (state.rotation == display::rotation::RIGHT)
    {
        rotation[0][0] = 0;
        rotation[0][1] = -1;
        rotation[1][0] = 1;
        rotation[1][1] = 0;
    }
    else if (state.rotation == display::rotation::INVERTED)
    {
        rotation[0][0] = -1;
        rotation[0][1] = 0;
        rotation[1][0] = 0;
        rotation[1][1] = -1;
    }
    else if (state.rotation == display::rotation::LEFT)
    {
        rotation[0][0] = 0;
        rotation[0][1] = 1;
        rotation[1][0] = -1;
        rotation[1][1] = 0;
    }

    float temp[3][3];
    multiply_matrices(temp, rotation, translate);
    multiply_matrices(transform_matrix, temp, scale);
}

bool map_tablet_to_output(std::string tablet_name, std::string output_name)
{
    x11::session x11;
    x11::screen_resources resources(x11);

    Screen *default_screen = XDefaultScreenOfDisplay(x11.display);
    x11::device_info devices(x11);

    XDeviceInfo *tablet_device_info = devices[tablet_name];

    if (!tablet_device_info)
        return false;

    for (size_t i = 0; i < resources->noutput; ++i)
    {
        x11::output_id output_id(x11, resources, i);
        x11::output_info output_info(x11, resources, output_id);

        if (output_info->connection != RR_Connected)
            continue;

        display::edid edid = get_edid(x11, resources->outputs[i]);

        if (output_name != (std::string)output_info->name &&
            output_name != edid.digest.hex())
            continue;

        display::output output = init_output(x11, resources, i);
        display::state state = output;

        x11::xi_device_info xi_device_info(x11, tablet_device_info->id);
        display::vec2<uint32_t> tablet_dimensions =
            xi_device_info.get_tablet_dimensions();
        if (tablet_dimensions.x == 0 || tablet_dimensions.y == 0)
            return false;

        float transform_matrix[3][3];
        generate_transform_matrix(state, transform_matrix);

        x11::x_device tablet_device(x11, tablet_device_info);

        return tablet_device.set_coodinate_transformation_matrix(transform_matrix);
    }
    return false;
}
