
//{{BLOCK(Terrain3)

//======================================================================
//
//	Terrain3, 512x256@8, 
//	+ palette 256 entries, not compressed
//	+ 32 tiles (t|f|p reduced) not compressed
//	+ regular map (in SBBs), not compressed, 64x32 
//	Total size: 512 + 2048 + 4096 = 6656
//
//	Time-stamp: 2020-08-22, 17:50:16
//	Exported by Cearn's GBA Image Transmogrifier, v0.8.6
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_TERRAIN3_H
#define GRIT_TERRAIN3_H

#define Terrain3TilesLen 2048
extern const unsigned short Terrain3Tiles[1024];

#define Terrain3MapLen 4096
extern const unsigned short Terrain3Map[2048];

#define Terrain3PalLen 512
extern const unsigned short Terrain3Pal[256];

#endif // GRIT_TERRAIN3_H

//}}BLOCK(Terrain3)
