// Doom editor numbers and initial sprite frames from code/info.cpp.
#pragma once
namespace de {
struct PreviewSprite {
    int type;
    const char *frame;
    bool fullbright, ceiling;
};
inline constexpr PreviewSprite previewSprites[] = {
    {3004, "POSSA", false, false}, {9, "SPOSA", false, false},     {64, "VILEA", false, false},
    {66, "SKELA", false, false},   {67, "FATTA", false, false},    {65, "CPOSA", false, false},
    {3001, "TROOA", false, false}, {3002, "SARGA", false, false},  {58, "SARGA", false, false},
    {3005, "HEADA", false, false}, {3003, "BOSSA", false, false},  {69, "BOS2A", false, false},
    {3006, "SKULA", true, false},  {7, "SPIDA", false, false},     {68, "BSPIA", false, false},
    {16, "CYBRA", false, false},   {71, "PAINA", false, false},    {84, "SSWVA", false, false},
    {72, "KEENA", false, true},    {88, "BBRNA", false, false},    {89, "SSWVA", false, false},
    {87, "TROOA", false, false},   {2035, "BAR1A", false, false},  {14, "TROOA", false, false},
    {2018, "ARM1A", false, false}, {2019, "ARM2A", false, false},  {2014, "BON1A", false, false},
    {2015, "BON2A", false, false}, {5, "BKEYA", false, false},     {13, "RKEYA", false, false},
    {6, "YKEYA", false, false},    {39, "YSKUA", false, false},    {38, "RSKUA", false, false},
    {40, "BSKUA", false, false},   {2011, "STIMA", false, false},  {2012, "MEDIA", false, false},
    {2013, "SOULA", true, false},  {2022, "PINVA", true, false},   {2023, "PSTRA", true, false},
    {2024, "PINSA", true, false},  {2025, "SUITA", true, false},   {2026, "PMAPA", true, false},
    {2045, "PVISA", true, false},  {83, "MEGAA", true, false},     {2007, "CLIPA", false, false},
    {2048, "AMMOA", false, false}, {2010, "ROCKA", false, false},  {2046, "BROKA", false, false},
    {2047, "CELLA", false, false}, {17, "CELPA", false, false},    {2008, "SHELA", false, false},
    {2049, "SBOXA", false, false}, {8, "BPAKA", false, false},     {2006, "BFUGA", false, false},
    {2002, "MGUNA", false, false}, {2005, "CSAWA", false, false},  {2003, "LAUNA", false, false},
    {2004, "PLASA", false, false}, {2001, "SHOTA", false, false},  {82, "SGN2A", false, false},
    {85, "TLMPA", true, false},    {86, "TLP2A", true, false},     {2028, "COLUA", true, false},
    {30, "COL1A", false, false},   {31, "COL2A", false, false},    {32, "COL3A", false, false},
    {33, "COL4A", false, false},   {37, "COL6A", false, false},    {36, "COL5A", false, false},
    {41, "CEYEA", true, false},    {42, "FSKUA", true, false},     {43, "TRE1A", false, false},
    {44, "TBLUA", true, false},    {45, "TGRNA", true, false},     {46, "TREDA", true, false},
    {55, "SMBTA", true, false},    {56, "SMGTA", true, false},     {57, "SMRTA", true, false},
    {47, "SMITA", false, false},   {48, "ELECA", false, false},    {34, "CANDA", true, false},
    {35, "CBRAA", true, false},    {49, "GOR1A", false, true},     {50, "GOR2A", false, true},
    {51, "GOR3A", false, true},    {52, "GOR4A", false, true},     {53, "GOR5A", false, true},
    {59, "GOR2A", false, true},    {60, "GOR4A", false, true},     {61, "GOR3A", false, true},
    {62, "GOR5A", false, true},    {63, "GOR1A", false, true},     {22, "HEADL", false, false},
    {15, "PLAYN", false, false},   {18, "POSSL", false, false},    {21, "SARGN", false, false},
    {23, "SKULK", false, false},   {20, "TROOM", false, false},    {19, "SPOSL", false, false},
    {10, "PLAYW", false, false},   {12, "PLAYW", false, false},    {28, "POL2A", false, false},
    {24, "POL5A", false, false},   {27, "POL4A", false, false},    {29, "POL3A", true, false},
    {25, "POL1A", false, false},   {26, "POL6A", false, false},    {54, "TRE2A", false, false},
    {70, "FCANA", true, false},    {73, "HDB1A", false, true},     {74, "HDB2A", false, true},
    {75, "HDB3A", false, true},    {76, "HDB4A", false, true},     {77, "HDB5A", false, true},
    {78, "HDB6A", false, true},    {79, "POB1A", false, false},    {80, "POB2A", false, false},
    {81, "BRS1A", false, false},   {32001, "SSWVA", false, false},
};
} // namespace de
