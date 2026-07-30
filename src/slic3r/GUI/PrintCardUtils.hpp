#ifndef slic3r_GUI_PrintCardUtils_hpp_
#define slic3r_GUI_PrintCardUtils_hpp_

#include <string>
#include <vector>

namespace Slic3r { namespace GUI {

// Data for a single "print settings card" (a.k.a. traveler / QA card). All
// values are pre-formatted, human-readable strings so the PDF renderer stays
// independent of the slicer config types. Empty strings are rendered as "-".
struct PrintCardData
{
    // Identity
    std::string title;            // project / model name
    std::string date;             // date the card was exported
    std::string printer_preset;   // selected printer preset name
    std::string process_preset;   // selected process/print preset name
    std::string filament_preset;  // selected filament preset name(s)
    std::string nozzle_diameter;  // e.g. "0.4 mm"
    int         plate_index = 1;  // 1-based plate number
    std::string thumbnail_path;   // PNG on disk; may be empty

    // Settings
    std::string layer_height;                 // e.g. "0.20 mm"
    std::string initial_layer_height;         // e.g. "0.20 mm" (optional)
    bool        variable_layer_height = false;
    std::string variable_layer_height_range;  // e.g. "0.08 - 0.28 mm" (optional)
    bool        ironing = false;
    std::string ironing_type;                 // e.g. "Top surfaces"
    std::string ironing_pattern;              // e.g. "Zig zag"
    std::string ironing_flow;                 // e.g. "10 %"
    std::string ironing_spacing;              // e.g. "0.10 mm"
    std::string ironing_inset;                // e.g. "0.10 mm" or "auto (half nozzle)"
    std::string ironing_speed;                // e.g. "30 mm/s"
    std::string ironing_direction;            // e.g. "45 deg"
    std::string ironing_skip_layers;          // e.g. "12 - 18", "12 - top", "none"
    std::string wall_loops;                   // e.g. "3"
    std::string sparse_infill_density;        // e.g. "15 %"
    std::string sparse_infill_pattern;        // e.g. "Grid" (optional)

    // Line widths, already resolved: a config value of zero means "use the
    // default line width", so these hold the effective width that was printed.
    std::string line_width_default;                // e.g. "0.42 mm"
    std::string line_width_initial_layer;
    std::string line_width_outer_wall;
    std::string line_width_inner_wall;
    std::string line_width_top_surface;
    std::string line_width_sparse_infill;
    std::string line_width_internal_solid_infill;
    std::string line_width_support;

    // Results (present after slicing)
    std::string print_time;       // e.g. "1h 23m"
    std::string filament_grams;   // e.g. "24.5 g"
    std::string filament_length;  // e.g. "8.2 m" (optional)
};

// Render a single-page A4 PDF traveler card to pdf_path. Returns true on
// success. Safe to call with a missing/empty thumbnail_path.
bool build_print_card_pdf(const std::string &pdf_path, const PrintCardData &data);

// One row of a layer-height sweep: the estimate for a single layer height.
// All values are pre-formatted; empty strings render as "-".
struct LayerHeightSweepRow
{
    std::string layer_height;     // e.g. "0.20 mm"
    std::string print_time;       // e.g. "7h 12m"
    std::string filament_grams;   // e.g. "182.4 g"
    std::string filament_length;  // e.g. "60.8 m"
    std::string objects;          // e.g. "60"
    std::string time_per_object;  // e.g. "7m 12s"
    std::string delta_vs_current; // e.g. "-32 %" relative to the starting height
    bool        is_current = false; // the layer height the plate started with
    bool        failed     = false; // this height produced no usable result
};

// A layer-height comparison report for one plate: the same plate re-sliced at
// each layer height with every other setting held constant.
struct LayerHeightSweepData
{
    std::string title;
    std::string date;
    std::string printer_preset;
    std::string process_preset;
    std::string filament_preset;
    std::string nozzle_diameter;
    int         plate_index = 1;
    std::string thumbnail_path;
    std::vector<LayerHeightSweepRow> rows;
};

// Render a single-page A4 PDF layer-height comparison table to pdf_path.
bool build_layer_height_sweep_pdf(const std::string &pdf_path, const LayerHeightSweepData &data);

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_PrintCardUtils_hpp_
