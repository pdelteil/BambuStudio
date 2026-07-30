#include "PrintCardUtils.hpp"

#include <algorithm>
#include <limits>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/log/trivial.hpp>

#include <hpdf/hpdf.h>

namespace Slic3r { namespace GUI {

// ----------------------------------------------------------------------------
// Small libharu helpers. These mirror the (file-static) helpers used by the
// assembly-steps PDF export so the two exporters stay independent.
// ----------------------------------------------------------------------------

// Load a Unicode-capable TTF/TTC font so titles / preset names render CJK
// correctly. Falls back to Helvetica. A failed font load poisons the whole
// document, so the error is reset after each failed attempt.
static HPDF_Font load_unicode_pdf_font(HPDF_Doc pdf)
{
    namespace fs = boost::filesystem;
    HPDF_UseUTFEncodings(pdf);

    struct TtcCandidate { const char *path; HPDF_UINT index; };
    static const TtcCandidate ttc_candidates[] = {
#if defined(_WIN32)
        {"C:\\Windows\\Fonts\\msyh.ttc",   0},
        {"C:\\Windows\\Fonts\\msyhl.ttc",  0},
        {"C:\\Windows\\Fonts\\simsun.ttc", 0},
#elif defined(__APPLE__)
        {"/System/Library/Fonts/PingFang.ttc",       0},
        {"/System/Library/Fonts/STHeiti Light.ttc",  0},
        {"/System/Library/Fonts/STHeiti Medium.ttc", 0},
#else
        {"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", 0},
        {"/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",         0},
        {"/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",           0},
#endif
    };
    static const char *ttf_candidates[] = {
#if defined(_WIN32)
        "C:\\Windows\\Fonts\\msyh.ttf",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\simsun.ttf",
        "C:\\Windows\\Fonts\\arialuni.ttf",
#elif defined(__APPLE__)
        "/Library/Fonts/Arial Unicode.ttf",
#else
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
#endif
    };

    for (const auto &c : ttc_candidates) {
        if (!fs::exists(c.path))
            continue;
        const char *fname = HPDF_LoadTTFontFromFile2(pdf, c.path, c.index, HPDF_TRUE);
        if (fname) {
            if (HPDF_Font f = HPDF_GetFont(pdf, fname, "UTF-8"))
                return f;
        }
        HPDF_ResetError(pdf);
    }
    for (const char *p : ttf_candidates) {
        if (!fs::exists(p))
            continue;
        const char *fname = HPDF_LoadTTFontFromFile(pdf, p, HPDF_TRUE);
        if (fname) {
            if (HPDF_Font f = HPDF_GetFont(pdf, fname, "UTF-8"))
                return f;
        }
        HPDF_ResetError(pdf);
    }

    HPDF_ResetError(pdf);
    return HPDF_GetFont(pdf, "Helvetica", nullptr);
}

static HPDF_Image load_pdf_image_from_file(HPDF_Doc pdf, const std::string &image_path)
{
    if (image_path.empty() || !boost::filesystem::exists(image_path))
        return nullptr;

    boost::nowide::ifstream image_file(image_path, std::ios::binary | std::ios::ate);
    if (!image_file)
        return nullptr;
    const std::streamoff sz = image_file.tellg();
    if (sz <= 0 || sz > static_cast<std::streamoff>(std::numeric_limits<HPDF_UINT>::max()))
        return nullptr;

    image_file.seekg(0, std::ios::beg);
    std::vector<HPDF_BYTE> buf(static_cast<size_t>(sz));
    image_file.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(sz));
    if (!image_file)
        return nullptr;

    if (boost::algorithm::iends_with(image_path, ".jpg") ||
        boost::algorithm::iends_with(image_path, ".jpeg"))
        return HPDF_LoadJpegImageFromMem(pdf, buf.data(), static_cast<HPDF_UINT>(buf.size()));
    return HPDF_LoadPngImageFromMem(pdf, buf.data(), static_cast<HPDF_UINT>(buf.size()));
}

static void draw_pdf_image_fit(HPDF_Page page, HPDF_Image image,
                               float x, float y, float w, float h)
{
    if (!page || !image || w <= 0.f || h <= 0.f)
        return;
    const float img_w = static_cast<float>(HPDF_Image_GetWidth(image));
    const float img_h = static_cast<float>(HPDF_Image_GetHeight(image));
    if (img_w <= 0.f || img_h <= 0.f)
        return;
    const float scale  = std::min(w / img_w, h / img_h);
    const float draw_w = img_w * scale;
    const float draw_h = img_h * scale;
    HPDF_Page_DrawImage(page, image,
        x + (w - draw_w) * 0.5f,
        y + (h - draw_h) * 0.5f,
        draw_w, draw_h);
}

static std::string pdf_fit_text_with_ellipsis(HPDF_Page page, const std::string &text, float max_width)
{
    if (!page || max_width <= 0.0f)
        return std::string();
    if (HPDF_Page_TextWidth(page, text.c_str()) <= max_width)
        return text;

    const std::string ellipsis = "...";
    if (HPDF_Page_TextWidth(page, ellipsis.c_str()) >= max_width)
        return ellipsis;

    std::string best;
    size_t i = 0;
    while (i < text.size()) {
        const unsigned char b = static_cast<unsigned char>(text[i]);
        size_t adv = 1;
        if      ((b & 0x80) == 0x00) adv = 1;
        else if ((b & 0xE0) == 0xC0) adv = 2;
        else if ((b & 0xF0) == 0xE0) adv = 3;
        else if ((b & 0xF8) == 0xF0) adv = 4;
        if (i + adv > text.size())
            break;
        const std::string candidate = text.substr(0, i + adv) + ellipsis;
        if (HPDF_Page_TextWidth(page, candidate.c_str()) > max_width)
            break;
        best = candidate;
        i += adv;
    }
    return best.empty() ? ellipsis : best;
}

// Fake-bold by double-striking with a tiny horizontal offset.
static void draw_pdf_bold_text(HPDF_Page page, float x, float y, const std::string &text)
{
    HPDF_Page_TextOut(page, x, y, text.c_str());
    HPDF_Page_TextOut(page, x + 0.4f, y, text.c_str());
}

// ----------------------------------------------------------------------------
// Card layout
// ----------------------------------------------------------------------------

namespace {

const char *dash_if_empty(const std::string &s) { return s.empty() ? "-" : s.c_str(); }

// One key/value line inside a section box. Returns the y of the next line.
// Type sizes for the card. Kept small on purpose: the card carries a lot of
// settings and has to stay on one A4 page.
const float CARD_FONT_SECTION = 9.0f;   // section title bars
const float CARD_FONT_ROW     = 8.5f;   // setting rows
const float CARD_ROW_GAP      = 4.0f;   // leading added to the font size
const float CARD_ROW_PITCH    = CARD_FONT_ROW + CARD_ROW_GAP;

float draw_kv(HPDF_Page page, HPDF_Font font, float x, float y,
              float label_w, float value_w, float size,
              const std::string &label, const std::string &value,
              float gap = CARD_ROW_GAP)
{
    HPDF_Page_BeginText(page);
    HPDF_Page_SetFontAndSize(page, font, size);
    HPDF_Page_TextOut(page, x, y, label.c_str());
    const std::string v = pdf_fit_text_with_ellipsis(page, dash_if_empty(value), value_w);
    draw_pdf_bold_text(page, x + label_w, y, v);
    HPDF_Page_EndText(page);
    return y - (size + gap);
}

// Section header + framed box. Caller draws the rows; header_h is where rows start.
void draw_section_frame(HPDF_Page page, HPDF_Font font, float x, float y_top,
                        float w, float h, const std::string &title)
{
    // Title bar
    HPDF_Page_SetRGBFill(page, 0.16f, 0.16f, 0.16f);
    HPDF_Page_Rectangle(page, x, y_top - 18.0f, w, 18.0f);
    HPDF_Page_Fill(page);
    HPDF_Page_SetRGBFill(page, 1.0f, 1.0f, 1.0f);
    HPDF_Page_BeginText(page);
    HPDF_Page_SetFontAndSize(page, font, CARD_FONT_SECTION);
    draw_pdf_bold_text(page, x + 6.0f, y_top - 13.0f, title);
    HPDF_Page_EndText(page);
    HPDF_Page_SetRGBFill(page, 0.0f, 0.0f, 0.0f);

    // Body frame
    HPDF_Page_SetLineWidth(page, 0.6f);
    HPDF_Page_SetRGBStroke(page, 0.6f, 0.6f, 0.6f);
    HPDF_Page_Rectangle(page, x, y_top - h, w, h - 18.0f);
    HPDF_Page_Stroke(page);
    HPDF_Page_SetRGBStroke(page, 0.0f, 0.0f, 0.0f);
}

} // namespace

bool build_print_card_pdf(const std::string &pdf_path, const PrintCardData &data)
{
    HPDF_Doc pdf = HPDF_New(nullptr, nullptr);
    if (!pdf)
        return false;

    HPDF_SetCompressionMode(pdf, HPDF_COMP_ALL);
    HPDF_Font font = load_unicode_pdf_font(pdf);

    HPDF_Page page = HPDF_AddPage(pdf);
    HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
    const float page_w = HPDF_Page_GetWidth(page);
    const float page_h = HPDF_Page_GetHeight(page);
    const float margin = 40.0f;
    const float content_w = page_w - 2.0f * margin;

    // --- Header: title + date ------------------------------------------------
    float y = page_h - margin;
    HPDF_Page_BeginText(page);
    HPDF_Page_SetFontAndSize(page, font, 20.0f);
    const std::string title = pdf_fit_text_with_ellipsis(
        page, data.title.empty() ? "Print settings card" : data.title, content_w - 140.0f);
    draw_pdf_bold_text(page, margin, y - 16.0f, title);
    HPDF_Page_SetFontAndSize(page, font, 9.0f);
    {
        const std::string dline = std::string("Date: ") + dash_if_empty(data.date);
        const float dw = HPDF_Page_TextWidth(page, dline.c_str());
        HPDF_Page_TextOut(page, page_w - margin - dw, y - 10.0f, dline.c_str());
        const std::string pline = "Plate " + std::to_string(data.plate_index);
        const float pw = HPDF_Page_TextWidth(page, pline.c_str());
        HPDF_Page_TextOut(page, page_w - margin - pw, y - 22.0f, pline.c_str());
    }
    HPDF_Page_EndText(page);

    // Divider
    y -= 26.0f;
    HPDF_Page_SetLineWidth(page, 1.0f);
    HPDF_Page_MoveTo(page, margin, y);
    HPDF_Page_LineTo(page, page_w - margin, y);
    HPDF_Page_Stroke(page);
    y -= 14.0f;

    // --- Top row: thumbnail (left) + identity (right) ------------------------
    const float thumb_w = 150.0f;
    const float thumb_h = 150.0f;
    const float top_y   = y;

    // Thumbnail box
    HPDF_Page_SetRGBStroke(page, 0.6f, 0.6f, 0.6f);
    HPDF_Page_SetLineWidth(page, 0.6f);
    HPDF_Page_Rectangle(page, margin, top_y - thumb_h, thumb_w, thumb_h);
    HPDF_Page_Stroke(page);
    HPDF_Page_SetRGBStroke(page, 0.0f, 0.0f, 0.0f);
    if (HPDF_Image thumb = load_pdf_image_from_file(pdf, data.thumbnail_path)) {
        draw_pdf_image_fit(page, thumb, margin + 4.0f, top_y - thumb_h + 4.0f,
                           thumb_w - 8.0f, thumb_h - 8.0f);
    } else {
        HPDF_Page_BeginText(page);
        HPDF_Page_SetFontAndSize(page, font, 9.0f);
        HPDF_Page_SetRGBFill(page, 0.5f, 0.5f, 0.5f);
        HPDF_Page_TextOut(page, margin + 34.0f, top_y - thumb_h * 0.5f, "(no preview)");
        HPDF_Page_SetRGBFill(page, 0.0f, 0.0f, 0.0f);
        HPDF_Page_EndText(page);
    }

    // Identity block
    const float id_x = margin + thumb_w + 20.0f;
    const float id_label_w = 95.0f;
    const float id_value_w = page_w - margin - id_x - id_label_w;
    float iy = top_y - 4.0f;
    iy = draw_kv(page, font, id_x, iy, id_label_w, id_value_w, 10.0f, "Printer",  data.printer_preset);
    iy = draw_kv(page, font, id_x, iy, id_label_w, id_value_w, 10.0f, "Nozzle",   data.nozzle_diameter);
    iy = draw_kv(page, font, id_x, iy, id_label_w, id_value_w, 10.0f, "Process",  data.process_preset);
    iy = draw_kv(page, font, id_x, iy, id_label_w, id_value_w, 10.0f, "Filament", data.filament_preset);
    iy -= 4.0f;
    // Results highlighted
    iy = draw_kv(page, font, id_x, iy, id_label_w, id_value_w, 11.0f, "Print time",    data.print_time);
    iy = draw_kv(page, font, id_x, iy, id_label_w, id_value_w, 11.0f, "Filament used", data.filament_grams);
    if (!data.filament_length.empty())
        iy = draw_kv(page, font, id_x, iy, id_label_w, id_value_w, 10.0f, "Length", data.filament_length);

    y = top_y - thumb_h - 22.0f;

    // --- Settings section ----------------------------------------------------
    const float sec_h = 5.0f * CARD_ROW_PITCH + 34.0f;
    draw_section_frame(page, font, margin, y, content_w, sec_h, "Print settings");
    {
        const float col_x   = margin + 8.0f;
        const float label_w = 130.0f;
        const float value_w = content_w - 16.0f - label_w;
        float ry = y - 30.0f;
        ry = draw_kv(page, font, col_x, ry, label_w, value_w, CARD_FONT_ROW, "Layer height",       data.layer_height);
        if (!data.initial_layer_height.empty())
            ry = draw_kv(page, font, col_x, ry, label_w, value_w, CARD_FONT_ROW, "First layer height", data.initial_layer_height);
        ry = draw_kv(page, font, col_x, ry, label_w, value_w, CARD_FONT_ROW, "Wall loops",         data.wall_loops);
        ry = draw_kv(page, font, col_x, ry, label_w, value_w, CARD_FONT_ROW, "Sparse infill",
                     data.sparse_infill_pattern.empty()
                         ? data.sparse_infill_density
                         : data.sparse_infill_density + "  (" + data.sparse_infill_pattern + ")");
        {
            std::string vlh = data.variable_layer_height ? "Yes" : "No";
            if (data.variable_layer_height && !data.variable_layer_height_range.empty())
                vlh += "  (" + data.variable_layer_height_range + ")";
            ry = draw_kv(page, font, col_x, ry, label_w, value_w, CARD_FONT_ROW, "Variable layer height", vlh);
        }
    }

    y -= sec_h + 20.0f;

    // --- Ironing section -----------------------------------------------------
    // Every ironing parameter, two columns of five rows. The values are shown
    // whether or not ironing is enabled: the first row says which it is, and
    // the rest document what would be (or was) used.
    const float iron_h = 5.0f * CARD_ROW_PITCH + 34.0f;
    draw_section_frame(page, font, margin, y, content_w, iron_h, "Ironing");
    {
        const float half      = content_w * 0.5f;
        const float lcol_x    = margin + 8.0f;
        const float rcol_x    = margin + 8.0f + half;
        const float ir_label  = 110.0f;
        const float ir_value  = half - 16.0f - ir_label;
        const float row_top   = y - 30.0f;

        std::string type = dash_if_empty(data.ironing_type);
        if (!data.ironing)
            type += "  (off)";

        // Type spans the full width: "Iron internal top surfaces only" is wider
        // than a single column can hold.
        const float full_value = content_w - 16.0f - ir_label;
        const float cols_top   = draw_kv(page, font, lcol_x, row_top, ir_label, full_value,
                                         CARD_FONT_ROW, "Type", type);

        float ly = cols_top;
        ly = draw_kv(page, font, lcol_x, ly, ir_label, ir_value, CARD_FONT_ROW, "Pattern", data.ironing_pattern);
        ly = draw_kv(page, font, lcol_x, ly, ir_label, ir_value, CARD_FONT_ROW, "Flow",    data.ironing_flow);
        ly = draw_kv(page, font, lcol_x, ly, ir_label, ir_value, CARD_FONT_ROW, "Spacing", data.ironing_spacing);
        ly = draw_kv(page, font, lcol_x, ly, ir_label, ir_value, CARD_FONT_ROW, "Inset",   data.ironing_inset);

        float ry = cols_top;
        ry = draw_kv(page, font, rcol_x, ry, ir_label, ir_value, CARD_FONT_ROW, "Speed",       data.ironing_speed);
        ry = draw_kv(page, font, rcol_x, ry, ir_label, ir_value, CARD_FONT_ROW, "Direction",   data.ironing_direction);
        ry = draw_kv(page, font, rcol_x, ry, ir_label, ir_value, CARD_FONT_ROW, "Skip layers", data.ironing_skip_layers);
    }

    y -= iron_h + 20.0f;

    // --- Line widths section -------------------------------------------------
    // Two columns of four rows; values are already resolved against the default
    // width by the caller, so each row is the width actually extruded.
    const float lw_h = 4.0f * CARD_ROW_PITCH + 34.0f;
    draw_section_frame(page, font, margin, y, content_w, lw_h, "Line widths");
    {
        const float half     = content_w * 0.5f;
        const float lcol_x   = margin + 8.0f;
        const float rcol_x   = margin + 8.0f + half;
        const float lw_label = 110.0f;
        const float lw_value = half - 16.0f - lw_label;
        const float row_top  = y - 30.0f;

        float ly = row_top;
        ly = draw_kv(page, font, lcol_x, ly, lw_label, lw_value, CARD_FONT_ROW, "Default",       data.line_width_default);
        ly = draw_kv(page, font, lcol_x, ly, lw_label, lw_value, CARD_FONT_ROW, "First layer",   data.line_width_initial_layer);
        ly = draw_kv(page, font, lcol_x, ly, lw_label, lw_value, CARD_FONT_ROW, "Outer wall",    data.line_width_outer_wall);
        ly = draw_kv(page, font, lcol_x, ly, lw_label, lw_value, CARD_FONT_ROW, "Inner wall",    data.line_width_inner_wall);

        float ry2 = row_top;
        ry2 = draw_kv(page, font, rcol_x, ry2, lw_label, lw_value, CARD_FONT_ROW, "Top surface",           data.line_width_top_surface);
        ry2 = draw_kv(page, font, rcol_x, ry2, lw_label, lw_value, CARD_FONT_ROW, "Sparse infill",         data.line_width_sparse_infill);
        ry2 = draw_kv(page, font, rcol_x, ry2, lw_label, lw_value, CARD_FONT_ROW, "Internal solid infill", data.line_width_internal_solid_infill);
        ry2 = draw_kv(page, font, rcol_x, ry2, lw_label, lw_value, CARD_FONT_ROW, "Support",               data.line_width_support);
    }

    y -= lw_h + 20.0f;

    // --- QA approval box -----------------------------------------------------
    const float qa_h = 96.0f;
    draw_section_frame(page, font, margin, y, content_w, qa_h, "Quality inspection");
    {
        const float bx = margin + 12.0f;
        float by = y - 40.0f;
        // Pass / Fail checkboxes
        HPDF_Page_SetLineWidth(page, 1.0f);
        HPDF_Page_Rectangle(page, bx, by - 3.0f, 12.0f, 12.0f);
        HPDF_Page_Stroke(page);
        HPDF_Page_Rectangle(page, bx + 120.0f, by - 3.0f, 12.0f, 12.0f);
        HPDF_Page_Stroke(page);
        HPDF_Page_BeginText(page);
        HPDF_Page_SetFontAndSize(page, font, 11.0f);
        draw_pdf_bold_text(page, bx + 18.0f, by, "PASS");
        draw_pdf_bold_text(page, bx + 138.0f, by, "FAIL");
        HPDF_Page_EndText(page);

        // Inspector / Date signature lines
        by -= 34.0f;
        HPDF_Page_BeginText(page);
        HPDF_Page_SetFontAndSize(page, font, 10.0f);
        HPDF_Page_TextOut(page, bx, by, "Inspector:");
        HPDF_Page_TextOut(page, bx + 300.0f, by, "Date:");
        HPDF_Page_EndText(page);
        HPDF_Page_SetLineWidth(page, 0.6f);
        HPDF_Page_MoveTo(page, bx + 62.0f,  by - 2.0f); HPDF_Page_LineTo(page, bx + 280.0f, by - 2.0f);
        HPDF_Page_MoveTo(page, bx + 335.0f, by - 2.0f); HPDF_Page_LineTo(page, content_w + margin - 12.0f, by - 2.0f);
        HPDF_Page_Stroke(page);
        HPDF_Page_BeginText(page);
        HPDF_Page_SetFontAndSize(page, font, 8.0f);
        HPDF_Page_TextOut(page, bx, by - 18.0f, "Notes:");
        HPDF_Page_EndText(page);
    }

    // --- Footer --------------------------------------------------------------
    HPDF_Page_BeginText(page);
    HPDF_Page_SetFontAndSize(page, font, 7.5f);
    HPDF_Page_SetRGBFill(page, 0.55f, 0.55f, 0.55f);
    HPDF_Page_TextOut(page, margin, margin - 12.0f,
                      "Generated by Bambu Studio - print settings card");
    HPDF_Page_SetRGBFill(page, 0.0f, 0.0f, 0.0f);
    HPDF_Page_EndText(page);

    const HPDF_STATUS st = HPDF_SaveToFile(pdf, pdf_path.c_str());
    HPDF_Free(pdf);
    if (st != HPDF_OK) {
        BOOST_LOG_TRIVIAL(error) << "print card export: HPDF_SaveToFile failed, status=" << st;
        return false;
    }
    return true;
}

bool build_layer_height_sweep_pdf(const std::string &pdf_path, const LayerHeightSweepData &data)
{
    HPDF_Doc pdf = HPDF_New(nullptr, nullptr);
    if (!pdf)
        return false;

    HPDF_SetCompressionMode(pdf, HPDF_COMP_ALL);
    HPDF_Font font = load_unicode_pdf_font(pdf);

    HPDF_Page page = HPDF_AddPage(pdf);
    HPDF_Page_SetSize(page, HPDF_PAGE_SIZE_A4, HPDF_PAGE_PORTRAIT);
    const float page_w = HPDF_Page_GetWidth(page);
    const float page_h = HPDF_Page_GetHeight(page);
    const float margin = 40.0f;
    const float content_w = page_w - 2.0f * margin;

    // --- Header --------------------------------------------------------------
    float y = page_h - margin;
    HPDF_Page_BeginText(page);
    HPDF_Page_SetFontAndSize(page, font, 20.0f);
    draw_pdf_bold_text(page, margin, y - 16.0f,
                       pdf_fit_text_with_ellipsis(page, "Layer height comparison", content_w - 140.0f));
    HPDF_Page_SetFontAndSize(page, font, 9.0f);
    {
        const std::string dline = std::string("Date: ") + dash_if_empty(data.date);
        const float dw = HPDF_Page_TextWidth(page, dline.c_str());
        HPDF_Page_TextOut(page, page_w - margin - dw, y - 10.0f, dline.c_str());
        const std::string pline = "Plate " + std::to_string(data.plate_index);
        const float pw = HPDF_Page_TextWidth(page, pline.c_str());
        HPDF_Page_TextOut(page, page_w - margin - pw, y - 22.0f, pline.c_str());
    }
    HPDF_Page_EndText(page);

    y -= 26.0f;
    HPDF_Page_SetLineWidth(page, 1.0f);
    HPDF_Page_MoveTo(page, margin, y);
    HPDF_Page_LineTo(page, page_w - margin, y);
    HPDF_Page_Stroke(page);
    y -= 14.0f;

    // --- Top row: thumbnail (left) + identity (right) ------------------------
    const float thumb_w = 120.0f;
    const float thumb_h = 120.0f;
    const float top_y   = y;

    HPDF_Page_SetRGBStroke(page, 0.6f, 0.6f, 0.6f);
    HPDF_Page_SetLineWidth(page, 0.6f);
    HPDF_Page_Rectangle(page, margin, top_y - thumb_h, thumb_w, thumb_h);
    HPDF_Page_Stroke(page);
    HPDF_Page_SetRGBStroke(page, 0.0f, 0.0f, 0.0f);
    if (HPDF_Image thumb = load_pdf_image_from_file(pdf, data.thumbnail_path))
        draw_pdf_image_fit(page, thumb, margin + 4.0f, top_y - thumb_h + 4.0f, thumb_w - 8.0f, thumb_h - 8.0f);

    const float id_x = margin + thumb_w + 20.0f;
    const float id_label_w = 95.0f;
    const float id_value_w = page_w - margin - id_x - id_label_w;
    float iy = top_y - 4.0f;
    iy = draw_kv(page, font, id_x, iy, id_label_w, id_value_w, 10.0f, "Model",    data.title);
    iy = draw_kv(page, font, id_x, iy, id_label_w, id_value_w, 10.0f, "Printer",  data.printer_preset);
    iy = draw_kv(page, font, id_x, iy, id_label_w, id_value_w, 10.0f, "Nozzle",   data.nozzle_diameter);
    iy = draw_kv(page, font, id_x, iy, id_label_w, id_value_w, 10.0f, "Process",  data.process_preset);
    iy = draw_kv(page, font, id_x, iy, id_label_w, id_value_w, 10.0f, "Filament", data.filament_preset);

    y = top_y - thumb_h - 22.0f;

    // --- Comparison table ----------------------------------------------------
    // Column x offsets relative to the left margin; the last entry is the right edge.
    const float col_x[8] = { 0.0f, 78.0f, 158.0f, 220.0f, 292.0f, 357.0f, 407.0f, content_w };
    const char *headers[7] = { "Layer height", "Print time", "vs current", "Filament", "Length", "Objects", "Time / object" };
    const int   n_cols = 7;
    const float row_h = 17.0f;

    // Header band
    HPDF_Page_SetRGBFill(page, 0.16f, 0.16f, 0.16f);
    HPDF_Page_Rectangle(page, margin, y - 18.0f, content_w, 18.0f);
    HPDF_Page_Fill(page);
    HPDF_Page_SetRGBFill(page, 1.0f, 1.0f, 1.0f);
    HPDF_Page_BeginText(page);
    HPDF_Page_SetFontAndSize(page, font, 9.0f);
    for (int c = 0; c < n_cols; ++c)
        draw_pdf_bold_text(page, margin + col_x[c] + 6.0f, y - 13.0f, headers[c]);
    HPDF_Page_EndText(page);
    HPDF_Page_SetRGBFill(page, 0.0f, 0.0f, 0.0f);

    float ry = y - 18.0f;
    for (size_t i = 0; i < data.rows.size(); ++i) {
        const LayerHeightSweepRow &r = data.rows[i];
        // Shade the row the plate started on so it is easy to find in the table.
        if (r.is_current) {
            HPDF_Page_SetRGBFill(page, 0.90f, 0.94f, 1.0f);
            HPDF_Page_Rectangle(page, margin, ry - row_h, content_w, row_h);
            HPDF_Page_Fill(page);
            HPDF_Page_SetRGBFill(page, 0.0f, 0.0f, 0.0f);
        } else if (i % 2 == 1) {
            HPDF_Page_SetRGBFill(page, 0.96f, 0.96f, 0.96f);
            HPDF_Page_Rectangle(page, margin, ry - row_h, content_w, row_h);
            HPDF_Page_Fill(page);
            HPDF_Page_SetRGBFill(page, 0.0f, 0.0f, 0.0f);
        }

        const float ty = ry - 12.0f;
        HPDF_Page_BeginText(page);
        HPDF_Page_SetFontAndSize(page, font, 9.0f);
        // Layer height, bold on the starting row
        {
            std::string lh = dash_if_empty(r.layer_height);
            if (r.is_current)
                lh += "  (current)";
            if (r.is_current)
                draw_pdf_bold_text(page, margin + col_x[0] + 6.0f, ty, lh);
            else
                HPDF_Page_TextOut(page, margin + col_x[0] + 6.0f, ty, lh.c_str());
        }
        if (r.failed) {
            HPDF_Page_SetRGBFill(page, 0.6f, 0.2f, 0.2f);
            HPDF_Page_TextOut(page, margin + col_x[1] + 6.0f, ty, "slicing failed");
            HPDF_Page_SetRGBFill(page, 0.0f, 0.0f, 0.0f);
        } else {
            const std::string cells[6] = { dash_if_empty(r.print_time), dash_if_empty(r.delta_vs_current),
                                           dash_if_empty(r.filament_grams), dash_if_empty(r.filament_length),
                                           dash_if_empty(r.objects), dash_if_empty(r.time_per_object) };
            for (int c = 0; c < n_cols - 1; ++c) {
                const std::string v = pdf_fit_text_with_ellipsis(page, cells[c], col_x[c + 2] - col_x[c + 1] - 10.0f);
                HPDF_Page_TextOut(page, margin + col_x[c + 1] + 6.0f, ty, v.c_str());
            }
        }
        HPDF_Page_EndText(page);
        ry -= row_h;
    }

    // Table frame + column rules
    HPDF_Page_SetLineWidth(page, 0.6f);
    HPDF_Page_SetRGBStroke(page, 0.6f, 0.6f, 0.6f);
    HPDF_Page_Rectangle(page, margin, ry, content_w, y - 18.0f - ry);
    HPDF_Page_Stroke(page);
    for (int c = 1; c < n_cols; ++c) {
        HPDF_Page_MoveTo(page, margin + col_x[c], y - 18.0f);
        HPDF_Page_LineTo(page, margin + col_x[c], ry);
    }
    HPDF_Page_Stroke(page);
    HPDF_Page_SetRGBStroke(page, 0.0f, 0.0f, 0.0f);

    // --- Method note ---------------------------------------------------------
    ry -= 22.0f;
    HPDF_Page_BeginText(page);
    HPDF_Page_SetFontAndSize(page, font, 8.0f);
    HPDF_Page_SetRGBFill(page, 0.35f, 0.35f, 0.35f);
    HPDF_Page_TextOut(page, margin, ry,
                      "Each row is a full slice of this plate with only the layer height changed; all other process settings were held constant.");
    HPDF_Page_TextOut(page, margin, ry - 11.0f,
                      "Per-object figures divide the plate total by the number of printable instances, so shared overhead is spread evenly.");
    HPDF_Page_SetRGBFill(page, 0.0f, 0.0f, 0.0f);
    HPDF_Page_EndText(page);

    // --- Footer --------------------------------------------------------------
    HPDF_Page_BeginText(page);
    HPDF_Page_SetFontAndSize(page, font, 7.5f);
    HPDF_Page_SetRGBFill(page, 0.55f, 0.55f, 0.55f);
    HPDF_Page_TextOut(page, margin, margin - 12.0f,
                      "Generated by Bambu Studio - layer height comparison");
    HPDF_Page_SetRGBFill(page, 0.0f, 0.0f, 0.0f);
    HPDF_Page_EndText(page);

    const HPDF_STATUS st = HPDF_SaveToFile(pdf, pdf_path.c_str());
    HPDF_Free(pdf);
    if (st != HPDF_OK) {
        BOOST_LOG_TRIVIAL(error) << "layer height sweep export: HPDF_SaveToFile failed, status=" << st;
        return false;
    }
    return true;
}

}} // namespace Slic3r::GUI
