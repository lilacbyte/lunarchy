#include "color_theme.h"
#include <sstream>
#include <algorithm>
#include <map>

static std::string trim(const std::string &s) {
	size_t start = s.find_first_not_of(" \t\r\n");
	size_t end = s.find_last_not_of(" \t\r\n");
	return (start == std::string::npos || end == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

static std::string toLower(const std::string &s) {
	std::string out = s;
	std::transform(out.begin(), out.end(), out.begin(), ::tolower);
	return out;
}

static video::SColor hslToSColor(int h, int s, int l, float a = 1.0f) {
	float hf = h / 360.0f;
	float sf = s / 100.0f;
	float lf = l / 100.0f;

	auto hueToRGB = [](float p, float q, float t) {
		if (t < 0) t += 1;
		if (t > 1) t -= 1;
		if (t < 1.0f / 6) return p + (q - p) * 6 * t;
		if (t < 1.0f / 2) return q;
		if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
		return p;
	};

	float r, g, b;
	if (sf == 0) {
		r = g = b = lf;
	} else {
		float q = lf < 0.5f ? lf * (1 + sf) : lf + sf - lf * sf;
		float p = 2 * lf - q;
		r = hueToRGB(p, q, hf + 1.0f / 3);
		g = hueToRGB(p, q, hf);
		b = hueToRGB(p, q, hf - 1.0f / 3);
	}

	return video::SColor(
		static_cast<u32>(a * 255.0f),
		static_cast<u32>(r * 255.0f),
		static_cast<u32>(g * 255.0f),
		static_cast<u32>(b * 255.0f)
	);
}

static video::SColor parseHSL(const std::string &value) {
	int h = 0, s = 0, l = 0;
	float a = 1.0f;

	if (value.find("hsla") == 0)
		sscanf(value.c_str(), "hsla(%d, %d%%, %d%%, %f)", &h, &s, &l, &a);
	else
		sscanf(value.c_str(), "hsl(%d, %d%%, %d%%)", &h, &s, &l);

	return hslToSColor(h, s, l, a);
}

ColorTheme::ColorTheme(const std::string &data) {
	std::istringstream stream(data);
	std::string line;

	std::map<std::string, video::SColor*> colorMap = {
		{"background-top",    &background_top},
		{"background",        &background},
		{"background-bottom", &background_bottom},
		{"border",            &border},
		{"text",              &text},
		{"text-muted",        &text_muted},
		{"primary",           &primary},
		{"primary-muted",     &primary_muted},
		{"secondary",         &secondary},
		{"secondary-muted",   &secondary_muted},
	};

	while (std::getline(stream, line)) {
		line = trim(line);
		if (line.empty() || line[0] == '#' || line[0] == '[')
			continue;

		size_t eq = line.find('=');
		if (eq == std::string::npos)
			continue;

		std::string key = toLower(trim(line.substr(0, eq)));
		std::string value = trim(line.substr(eq + 1));

		if (key == "name") {
			name = value;
		} else if (colorMap.count(key)) {
			*colorMap[key] = parseHSL(value);
		}
	}
}

void ThemeManager::LoadThemes(const std::string &folderpath) {
    (void)folderpath;
    themes.clear();

    static const char *builtin_themes[] = {
R"THEME([theme]
name = Legacy

background-top    = hsl(220, 13%, 3%)
background        = hsl(220, 13%, 7%)
background-bottom = hsl(220, 13%, 10%)

border            = hsl(210, 90%, 40%)

text              = hsl(0, 0%, 100%)
text-muted        = hsl(0, 0%, 70%)

primary           = hsl(210, 90%, 40%)
primary-muted     = hsl(210, 90%, 30%)

secondary         = hsl(0, 90%, 30%)
secondary-muted   = hsl(0, 90%, 15%)
)THEME",
R"THEME([theme]
name = Midnight

background-top    = hsl(345, 20%, 1%)
background        = hsl(345, 20%, 2%)
background-bottom = hsl(345, 20%, 4%)

border            = hsl(345, 20%, 10%)

text              = hsl(0, 0%, 90%)
text-muted        = hsl(0, 0%, 60%)

primary           = hsl(350, 50%, 14%)
primary-muted     = hsl(350, 30%, 7%)

secondary         = hsl(10, 80%, 15%)
secondary-muted   = hsl(10, 50%, 7%)
)THEME",
R"THEME([theme]
name = Modern

background-top    = hsl(220, 13%, 2%)
background        = hsl(220, 13%, 4%)
background-bottom = hsl(220, 13%, 6%)

border            = hsl(220, 13%, 20%)

text              = hsl(0, 0%, 100%)
text-muted        = hsl(0, 0%, 70%)

primary           = hsl(220, 13%, 25%)
primary-muted     = hsl(220, 13%, 15%)

secondary         = hsl(220, 13%, 30%)
secondary-muted   = hsl(220, 13%, 15%)
)THEME",
R"THEME([theme]
name = Modern Light

background-top    = hsl(0, 0%, 98%)
background        = hsl(0, 0%, 96%)
background-bottom = hsl(0, 0%, 93%)

border            = hsl(0, 0%, 80%)

text              = hsl(220, 15%, 10%)
text-muted        = hsl(220, 10%, 50%)

primary           = hsl(220, 5%, 40%)
primary-muted     = hsl(220, 5%, 85%)

secondary         = hsl(0, 0%, 90%)
secondary-muted   = hsl(220, 5%, 50%)
)THEME",
R"THEME([theme]
name = Moss

background-top    = hsl(120, 10%, 8%)
background        = hsl(120, 10%, 10%)
background-bottom = hsl(120, 10%, 12%)

border            = hsl(120, 10%, 25%)

text              = hsl(0, 0%, 90%)
text-muted        = hsl(0, 0%, 60%)

primary           = hsl(120, 40%, 30%)
primary-muted     = hsl(120, 30%, 18%)

secondary         = hsl(100, 50%, 35%)
secondary-muted   = hsl(100, 40%, 20%)
)THEME",
R"THEME([theme]
name = Ocean

background-top    = hsl(200, 60%, 96%)
background        = hsl(200, 60%, 94%)
background-bottom = hsl(200, 60%, 91%)

border            = hsl(200, 30%, 70%)

text              = hsl(210, 30%, 15%)
text-muted        = hsl(210, 20%, 50%)

primary           = hsl(195, 85%, 45%)
primary-muted     = hsl(195, 70%, 80%)

secondary         = hsl(200, 60%, 40%)
secondary-muted   = hsl(210, 80%, 85%)
)THEME",
R"THEME([theme]
name = Outdoors

background-top    = hsl(80, 2%, 92%)
background        = hsl(80, 2%, 89%)
background-bottom = hsl(80, 2%, 86%)

border            = hsl(80, 5%, 80%)

text              = hsl(0, 0%, 12%)
text-muted        = hsl(0, 0%, 50%)

primary           = hsl(100, 60%, 35%)
primary-muted     = hsl(100, 60%, 75%)

secondary         = hsl(140, 50%, 35%)
secondary-muted   = hsl(140, 50%, 70%)
)THEME",
R"THEME([theme]
name = Rainbow

background-top    = hsl(220, 13%, 3%)
background        = hsl(220, 13%, 7%)
background-bottom = hsl(220, 13%, 10%)

border            = hsl(210, 90%, 40%)

text              = hsl(0, 0%, 100%)
text-muted        = hsl(0, 0%, 70%)

primary           = hsl(210, 90%, 40%)
primary-muted     = hsl(210, 90%, 30%)

secondary         = hsl(0, 90%, 30%)
secondary-muted   = hsl(0, 90%, 15%)
)THEME"
    };

    for (const char *theme_data : builtin_themes) {
        ColorTheme theme(theme_data);
        if (!theme.name.empty())
            themes.push_back(theme);
    }
}


std::vector<std::string> ThemeManager::GetThemes() const {
	std::vector<std::string> names;
	for (const auto &theme : themes)
		names.push_back(theme.name);
	return names;
}

ColorTheme ThemeManager::GetThemeByName(const std::string &name) const {
	std::string target = toLower(name);
	for (const auto &theme : themes) {
		if (toLower(theme.name) == target)
			return theme;
	}

	// Return a default theme with all black colors and white text
	ColorTheme fallback;
	fallback.name = "Fallback Theme";
	fallback.background_top = fallback.background = fallback.background_bottom =
	fallback.border = fallback.primary = fallback.primary_muted = fallback.secondary = fallback.secondary_muted = video::SColor(255, 0, 0, 0);
	fallback.text = fallback.text_muted = video::SColor(255, 255, 255, 255);
	return fallback;
}
