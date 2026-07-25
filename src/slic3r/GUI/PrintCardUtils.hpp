#ifndef slic3r_GUI_PrintCardUtils_hpp_
#define slic3r_GUI_PrintCardUtils_hpp_

#include <string>

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
    std::string ironing_flow;                 // e.g. "10 %"
    std::string ironing_spacing;              // e.g. "0.10 mm"
    std::string ironing_speed;                // e.g. "30 mm/s"
    std::string wall_loops;                   // e.g. "3"
    std::string sparse_infill_density;        // e.g. "15 %"
    std::string sparse_infill_pattern;        // e.g. "Grid" (optional)

    // Results (present after slicing)
    std::string print_time;       // e.g. "1h 23m"
    std::string filament_grams;   // e.g. "24.5 g"
    std::string filament_length;  // e.g. "8.2 m" (optional)
};

// Render a single-page A4 PDF traveler card to pdf_path. Returns true on
// success. Safe to call with a missing/empty thumbnail_path.
bool build_print_card_pdf(const std::string &pdf_path, const PrintCardData &data);

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_PrintCardUtils_hpp_
