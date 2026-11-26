#include "window.h"
#include <FL/Fl.H>
#include <FL/Fl_SVG_Image.H>
#include "assets.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <FL/fl_ask.H>
#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char **argv)
{
    const auto started = std::chrono::steady_clock::now();
    bool smoke = false;
    std::filesystem::path settings = pgjr::configDirectory() / "settings.json", report;
    std::string requested_language;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--settings-file" && i + 1 < argc)
            settings = std::filesystem::u8path(argv[++i]);
        else if (arg == "--startup-check")
            smoke = true;
        else if (arg == "--startup-report" && i + 1 < argc)
            report = std::filesystem::u8path(argv[++i]);
        else if (arg == "--language" && i + 1 < argc)
            requested_language = argv[++i];
        else if (arg == "--version")
        {
            std::cout << "penthario " << PGADM_APP_VERSION << '\n';
            return 0;
        }
        else
        {
            std::cerr << "Unknown or incomplete argument: " << arg << '\n';
            return 1;
        }
    }
    try
    {
        std::string locale = "nl";
        if (std::filesystem::exists(settings))
        {
            try
            {
                locale = pgjr::Json::parse(pgjr::readFile(settings)).value("language", "nl");
            }
            catch (...)
            {
            }
        }
        pgjr::language(requested_language.empty() ? locale : requested_language);
        Fl::lock();
        FLTK_Css css;
        pgjr::Window window(css, settings, smoke);
        Fl_SVG_Image svg(nullptr, appSvg);
        std::unique_ptr<Fl_Image> image(svg.copy(64, 64));
        if (auto *rgb = dynamic_cast<Fl_RGB_Image *>(image.get()))
            window.icon(rgb);
        else
            throw std::runtime_error("Application icon could not be rendered");
        window.show();
        if (smoke)
        {
            std::string diagnostics;
#ifdef _WIN32
            diagnostics = std::string("Console window: ") + (GetConsoleWindow() ? "present" : "none") + "\n";
#endif
            if (!report.empty())
                pgjr::writeFile(report,
                                std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                   std::chrono::steady_clock::now() - started)
                                                   .count()) +
                                    " ms to window construction\nApplication version: " + PGADM_APP_VERSION +
                                    "\nToolkit: FLTK\nLanguage: " + pgjr::language() + "\n" + diagnostics);
            Fl::add_timeout(0.2, [](void *p) { static_cast<pgjr::Window *>(p)->hide(); }, &window);
        }
        return Fl::run();
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        if (!smoke)
            fl_alert("Penthario could not start:\n%s", error.what());
        return 1;
    }
}
