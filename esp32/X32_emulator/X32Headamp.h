/*
 * X32Headamp.h
 *
 *  Created on: 4 févr. 2015
 *      Author: patrick
 */

#ifndef X32HEADAMP_H_
#define X32HEADAMP_H_

static const X32command Xheadamp_flash[] = {
		{"/headamp",							{HAMP}, F_FND, {0}, NULL},
		{"/headamp/000",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/000/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/000/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp = NULL;
static const X32command Xheadamp001_flash[] = {
		{"/headamp/001",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/001/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/001/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp001 = NULL;
static const X32command Xheadamp002_flash[] = {
		{"/headamp/002",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/002/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/002/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp002 = NULL;
static const X32command Xheadamp003_flash[] = {
		{"/headamp/003",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/003/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/003/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp003 = NULL;
static const X32command Xheadamp004_flash[] = {
		{"/headamp/004",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/004/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/004/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp004 = NULL;
static const X32command Xheadamp005_flash[] = {
		{"/headamp/005",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/005/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/005/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp005 = NULL;
static const X32command Xheadamp006_flash[] = {
		{"/headamp/006",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/006/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/006/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp006 = NULL;
static const X32command Xheadamp007_flash[] = {
		{"/headamp/007",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/007/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/007/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp007 = NULL;
static const X32command Xheadamp008_flash[] = {
		{"/headamp/008",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/008/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/008/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp008 = NULL;
static const X32command Xheadamp009_flash[] = {
		{"/headamp/009",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/009/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/009/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp009 = NULL;
static const X32command Xheadamp010_flash[] = {
		{"/headamp/010",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/010/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/010/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp010 = NULL;
static const X32command Xheadamp011_flash[] = {
		{"/headamp/011",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/011/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/011/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp011 = NULL;
static const X32command Xheadamp012_flash[] = {
		{"/headamp/012",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/012/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/012/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp012 = NULL;
static const X32command Xheadamp013_flash[] = {
		{"/headamp/013",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/013/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/013/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp013 = NULL;
static const X32command Xheadamp014_flash[] = {
		{"/headamp/014",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/014/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/014/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp014 = NULL;
static const X32command Xheadamp015_flash[] = {
		{"/headamp/015",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/015/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/015/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp015 = NULL;
static const X32command Xheadamp016_flash[] = {
		{"/headamp/016",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/016/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/016/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp016 = NULL;
static const X32command Xheadamp017_flash[] = {
		{"/headamp/017",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/017/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/017/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp017 = NULL;
static const X32command Xheadamp018_flash[] = {
		{"/headamp/018",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/018/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/018/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp018 = NULL;
static const X32command Xheadamp019_flash[] = {
		{"/headamp/019",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/019/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/019/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp019 = NULL;
static const X32command Xheadamp020_flash[] = {
		{"/headamp/020",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/020/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/020/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp020 = NULL;
static const X32command Xheadamp021_flash[] = {
		{"/headamp/021",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/021/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/021/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp021 = NULL;
static const X32command Xheadamp022_flash[] = {
		{"/headamp/022",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/022/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/022/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp022 = NULL;
static const X32command Xheadamp023_flash[] = {
		{"/headamp/023",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/023/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/023/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp023 = NULL;
static const X32command Xheadamp024_flash[] = {
		{"/headamp/024",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/024/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/024/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp024 = NULL;
static const X32command Xheadamp025_flash[] = {
		{"/headamp/025",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/025/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/025/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp025 = NULL;
static const X32command Xheadamp026_flash[] = {
		{"/headamp/026",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/026/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/026/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp026 = NULL;
static const X32command Xheadamp027_flash[] = {
		{"/headamp/027",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/027/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/027/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp027 = NULL;
static const X32command Xheadamp028_flash[] = {
		{"/headamp/028",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/028/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/028/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp028 = NULL;
static const X32command Xheadamp029_flash[] = {
		{"/headamp/029",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/029/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/029/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp029 = NULL;
static const X32command Xheadamp030_flash[] = {
		{"/headamp/030",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/030/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/030/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp030 = NULL;
static const X32command Xheadamp031_flash[] = {
		{"/headamp/031",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/031/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/031/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp031 = NULL;
static const X32command Xheadamp032_flash[] = {
		{"/headamp/032",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/032/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/032/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp032 = NULL;
static const X32command Xheadamp033_flash[] = {
		{"/headamp/033",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/033/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/033/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp033 = NULL;
static const X32command Xheadamp034_flash[] = {
		{"/headamp/034",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/034/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/034/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp034 = NULL;
static const X32command Xheadamp035_flash[] = {
		{"/headamp/035",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/035/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/035/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp035 = NULL;
static const X32command Xheadamp036_flash[] = {
		{"/headamp/036",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/036/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/036/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp036 = NULL;
static const X32command Xheadamp037_flash[] = {
		{"/headamp/037",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/037/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/037/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp037 = NULL;
static const X32command Xheadamp038_flash[] = {
		{"/headamp/038",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/038/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/038/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp038 = NULL;
static const X32command Xheadamp039_flash[] = {
		{"/headamp/039",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/039/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/039/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp039 = NULL;
static const X32command Xheadamp040_flash[] = {
		{"/headamp/040",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/040/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/040/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp040 = NULL;
static const X32command Xheadamp041_flash[] = {
		{"/headamp/041",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/041/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/041/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp041 = NULL;
static const X32command Xheadamp042_flash[] = {
		{"/headamp/042",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/042/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/042/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp042 = NULL;
static const X32command Xheadamp043_flash[] = {
		{"/headamp/043",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/043/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/043/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp043 = NULL;
static const X32command Xheadamp044_flash[] = {
		{"/headamp/044",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/044/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/044/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp044 = NULL;
static const X32command Xheadamp045_flash[] = {
		{"/headamp/045",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/045/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/045/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp045 = NULL;
static const X32command Xheadamp046_flash[] = {
		{"/headamp/046",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/046/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/046/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp046 = NULL;
static const X32command Xheadamp047_flash[] = {
		{"/headamp/047",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/047/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/047/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp047 = NULL;
static const X32command Xheadamp048_flash[] = {
		{"/headamp/048",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/048/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/048/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp048 = NULL;
static const X32command Xheadamp049_flash[] = {
		{"/headamp/049",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/049/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/049/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp049 = NULL;
static const X32command Xheadamp050_flash[] = {
		{"/headamp/050",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/050/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/050/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp050 = NULL;
static const X32command Xheadamp051_flash[] = {
		{"/headamp/051",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/051/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/051/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp051 = NULL;
static const X32command Xheadamp052_flash[] = {
		{"/headamp/052",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/052/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/052/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp052 = NULL;
static const X32command Xheadamp053_flash[] = {
		{"/headamp/053",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/053/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/053/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp053 = NULL;
static const X32command Xheadamp054_flash[] = {
		{"/headamp/054",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/054/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/054/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp054 = NULL;
static const X32command Xheadamp055_flash[] = {
		{"/headamp/055",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/055/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/055/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp055 = NULL;
static const X32command Xheadamp056_flash[] = {
		{"/headamp/056",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/056/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/056/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp056 = NULL;
static const X32command Xheadamp057_flash[] = {
		{"/headamp/057",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/057/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/057/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp057 = NULL;
static const X32command Xheadamp058_flash[] = {
		{"/headamp/058",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/058/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/058/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp058 = NULL;
static const X32command Xheadamp059_flash[] = {
		{"/headamp/059",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/059/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/059/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp059 = NULL;
static const X32command Xheadamp060_flash[] = {
		{"/headamp/060",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/060/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/060/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp060 = NULL;
static const X32command Xheadamp061_flash[] = {
		{"/headamp/061",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/061/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/061/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp061 = NULL;
static const X32command Xheadamp062_flash[] = {
		{"/headamp/062",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/062/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/062/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp062 = NULL;
static const X32command Xheadamp063_flash[] = {
		{"/headamp/063",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/063/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/063/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp063 = NULL;
static const X32command Xheadamp064_flash[] = {
		{"/headamp/064",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/064/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/064/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp064 = NULL;
static const X32command Xheadamp065_flash[] = {
		{"/headamp/065",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/065/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/065/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp065 = NULL;
static const X32command Xheadamp066_flash[] = {
		{"/headamp/066",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/066/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/066/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp066 = NULL;
static const X32command Xheadamp067_flash[] = {
		{"/headamp/067",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/067/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/067/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp067 = NULL;
static const X32command Xheadamp068_flash[] = {
		{"/headamp/068",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/068/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/068/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp068 = NULL;
static const X32command Xheadamp069_flash[] = {
		{"/headamp/069",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/069/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/069/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp069 = NULL;
static const X32command Xheadamp070_flash[] = {
		{"/headamp/070",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/070/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/070/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp070 = NULL;
static const X32command Xheadamp071_flash[] = {
		{"/headamp/071",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/071/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/071/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp071 = NULL;
static const X32command Xheadamp072_flash[] = {
		{"/headamp/072",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/072/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/072/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp072 = NULL;
static const X32command Xheadamp073_flash[] = {
		{"/headamp/073",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/073/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/073/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp073 = NULL;
static const X32command Xheadamp074_flash[] = {
		{"/headamp/074",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/074/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/074/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp074 = NULL;
static const X32command Xheadamp075_flash[] = {
		{"/headamp/075",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/075/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/075/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp075 = NULL;
static const X32command Xheadamp076_flash[] = {
		{"/headamp/076",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/076/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/076/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp076 = NULL;
static const X32command Xheadamp077_flash[] = {
		{"/headamp/077",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/077/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/077/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp077 = NULL;
static const X32command Xheadamp078_flash[] = {
		{"/headamp/078",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/078/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/078/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp078 = NULL;
static const X32command Xheadamp079_flash[] = {
		{"/headamp/079",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/079/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/079/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp079 = NULL;
static const X32command Xheadamp080_flash[] = {
		{"/headamp/080",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/080/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/080/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp080 = NULL;
static const X32command Xheadamp081_flash[] = {
		{"/headamp/081",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/081/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/081/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp081 = NULL;
static const X32command Xheadamp082_flash[] = {
		{"/headamp/082",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/082/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/082/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp082 = NULL;
static const X32command Xheadamp083_flash[] = {
		{"/headamp/083",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/083/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/083/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp083 = NULL;
static const X32command Xheadamp084_flash[] = {
		{"/headamp/084",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/084/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/084/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp084 = NULL;
static const X32command Xheadamp085_flash[] = {
		{"/headamp/085",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/085/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/085/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp085 = NULL;
static const X32command Xheadamp086_flash[] = {
		{"/headamp/086",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/086/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/086/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp086 = NULL;
static const X32command Xheadamp087_flash[] = {
		{"/headamp/087",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/087/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/087/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp087 = NULL;
static const X32command Xheadamp088_flash[] = {
		{"/headamp/088",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/088/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/088/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp088 = NULL;
static const X32command Xheadamp089_flash[] = {
		{"/headamp/089",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/089/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/089/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp089 = NULL;
static const X32command Xheadamp090_flash[] = {
		{"/headamp/090",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/090/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/090/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp090 = NULL;
static const X32command Xheadamp091_flash[] = {
		{"/headamp/091",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/091/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/091/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp091 = NULL;
static const X32command Xheadamp092_flash[] = {
		{"/headamp/092",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/092/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/092/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp092 = NULL;
static const X32command Xheadamp093_flash[] = {
		{"/headamp/093",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/093/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/093/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp093 = NULL;
static const X32command Xheadamp094_flash[] = {
		{"/headamp/094",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/094/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/094/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp094 = NULL;
static const X32command Xheadamp095_flash[] = {
		{"/headamp/095",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/095/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/095/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp095 = NULL;
static const X32command Xheadamp096_flash[] = {
		{"/headamp/096",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/096/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/096/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp096 = NULL;
static const X32command Xheadamp097_flash[] = {
		{"/headamp/097",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/097/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/097/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp097 = NULL;
static const X32command Xheadamp098_flash[] = {
		{"/headamp/098",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/098/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/098/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp098 = NULL;
static const X32command Xheadamp099_flash[] = {
		{"/headamp/099",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/099/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/099/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp099 = NULL;
static const X32command Xheadamp100_flash[] = {
		{"/headamp/100",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/100/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/100/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp100 = NULL;
static const X32command Xheadamp101_flash[] = {
		{"/headamp/101",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/101/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/101/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp101 = NULL;
static const X32command Xheadamp102_flash[] = {
		{"/headamp/102",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/102/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/102/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp102 = NULL;
static const X32command Xheadamp103_flash[] = {
		{"/headamp/103",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/103/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/103/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp103 = NULL;
static const X32command Xheadamp104_flash[] = {
		{"/headamp/104",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/104/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/104/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp104 = NULL;
static const X32command Xheadamp105_flash[] = {
		{"/headamp/105",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/105/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/105/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp105 = NULL;
static const X32command Xheadamp106_flash[] = {
		{"/headamp/106",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/106/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/106/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp106 = NULL;
static const X32command Xheadamp107_flash[] = {
		{"/headamp/107",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/107/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/107/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp107 = NULL;
static const X32command Xheadamp108_flash[] = {
		{"/headamp/108",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/108/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/108/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp108 = NULL;
static const X32command Xheadamp109_flash[] = {
		{"/headamp/109",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/109/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/109/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp109 = NULL;
static const X32command Xheadamp110_flash[] = {
		{"/headamp/110",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/110/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/110/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp110 = NULL;
static const X32command Xheadamp111_flash[] = {
		{"/headamp/111",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/111/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/111/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp111 = NULL;
static const X32command Xheadamp112_flash[] = {
		{"/headamp/112",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/112/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/112/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp112 = NULL;
static const X32command Xheadamp113_flash[] = {
		{"/headamp/113",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/113/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/113/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp113 = NULL;
static const X32command Xheadamp114_flash[] = {
		{"/headamp/114",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/114/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/114/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp114 = NULL;
static const X32command Xheadamp115_flash[] = {
		{"/headamp/115",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/115/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/115/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp115 = NULL;
static const X32command Xheadamp116_flash[] = {
		{"/headamp/116",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/116/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/116/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp116 = NULL;
static const X32command Xheadamp117_flash[] = {
		{"/headamp/117",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/117/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/117/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp117 = NULL;
static const X32command Xheadamp118_flash[] = {
		{"/headamp/118",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/118/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/118/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp118 = NULL;
static const X32command Xheadamp119_flash[] = {
		{"/headamp/119",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/119/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/119/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp119 = NULL;
static const X32command Xheadamp120_flash[] = {
		{"/headamp/120",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/120/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/120/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp120 = NULL;
static const X32command Xheadamp121_flash[] = {
		{"/headamp/121",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/121/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/121/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp121 = NULL;
static const X32command Xheadamp122_flash[] = {
		{"/headamp/122",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/122/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/122/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp122 = NULL;
static const X32command Xheadamp123_flash[] = {
		{"/headamp/123",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/123/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/123/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp123 = NULL;
static const X32command Xheadamp124_flash[] = {
		{"/headamp/124",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/124/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/124/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp124 = NULL;
static const X32command Xheadamp125_flash[] = {
		{"/headamp/125",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/125/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/125/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp125 = NULL;
static const X32command Xheadamp126_flash[] = {
		{"/headamp/126",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/126/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/126/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp126 = NULL;
static const X32command Xheadamp127_flash[] = {
		{"/headamp/127",						{HAMP}, F_FND, {0}, NULL},
		{"/headamp/127/gain",				{F32}, F_XET, {0}, NULL},
		{"/headamp/127/phantom",			{E32}, F_XET, {0}, OffOn},
};
X32command *Xheadamp127 = NULL;
const int Xheadamp_max = sizeof(Xheadamp_flash) / sizeof(X32command);
int Xheadamp1_max = sizeof(Xheadamp001) / sizeof(X32command);

X32command	*Xheadmpset[128];

#endif /* X32HEADAMP_H_ */
