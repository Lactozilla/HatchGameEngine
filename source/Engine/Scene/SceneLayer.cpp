#if INTERFACE

#include <Engine/Includes/Standard.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Scene/ScrollingInfo.h>
#include <Engine/Scene/ScrollingIndex.h>

class SceneLayer {
public:
    char              Name[50];
    bool              Visible = true;

    int               Width = 0;
    int               Height = 0;
    Uint32            WidthMask = 0;
    Uint32            HeightMask = 0;
    Uint32            WidthInBits = 0;
    Uint32            HeightInBits = 0;
    Uint32            WidthData = 0;
    Uint32            HeightData = 0;
    Uint32            DataSize = 0;
    Uint32            ScrollIndexCount = 0;

    int               RelativeY = 0x0100;
    int               ConstantY = 0x0000;
    int               OffsetX = 0x0000;
    int               OffsetY = 0x0000;

    Uint32*           Tiles = NULL;
    Uint32*           TilesBackup = NULL;
    Uint16*           TileOffsetY = NULL;

    int               DeformOffsetA = 0;
    int               DeformOffsetB = 0;
    int               DeformSetA[MAX_DEFORM_LINES];
    int               DeformSetB[MAX_DEFORM_LINES];
    int               DeformSplitLine = 0;

    int               Flags = 0x0000;
    int               DrawGroup = 0;
    Uint8             DrawBehavior = 0;

    HashMap<VMValue>* Properties = NULL;

    bool              Blending = false;
    Uint8             BlendMode = 0; // BlendMode_NORMAL
    float             Opacity = 1.0f;

    bool              UsingCustomScanlineFunction = false;
    ObjFunction       CustomScanlineFunction;

    int               ScrollInfoCount = 0;
    ScrollingInfo*    ScrollInfos = NULL;
    int               ScrollInfosSplitIndexesCount = 0;
    Uint16*           ScrollInfosSplitIndexes = NULL;
    Uint8*            ScrollIndexes = NULL;

    Uint32            BufferID = 0;
    int               VertexCount = 0;
    void*             TileBatches = NULL;

    enum {
        FLAGS_COLLIDEABLE = 1,
        FLAGS_REPEAT_X = 2,
        FLAGS_REPEAT_Y = 4,
    };
};
#endif

#include <Engine/Scene/SceneLayer.h>
#include <Engine/Rendering/Enums.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Math/Math.h>

PUBLIC         SceneLayer::SceneLayer() {

}
PUBLIC         SceneLayer::SceneLayer(int w, int h) {
    Width = w;
    Height = h;

    w = Math::CeilPOT(w);
    WidthMask = w - 1;
    WidthInBits = 0;
    for (int f = w >> 1; f > 0; f >>= 1)
        WidthInBits++;

    h = Math::CeilPOT(h);
    HeightMask = h - 1;
    HeightInBits = 0;
    for (int f = h >> 1; f > 0; f >>= 1)
        HeightInBits++;

    WidthData = w;
    HeightData = h;
    DataSize = w * h * sizeof(Uint32);

    ScrollIndexCount = WidthData;
    if (ScrollIndexCount < HeightData)
        ScrollIndexCount = HeightData;

    memset(DeformSetA, 0, sizeof(DeformSetA));
    memset(DeformSetB, 0, sizeof(DeformSetB));

    Tiles = (Uint32*)Memory::TrackedCalloc("SceneLayer::Tiles", w * h, sizeof(Uint32));
    TilesBackup = (Uint32*)Memory::TrackedCalloc("SceneLayer::TilesBackup", w * h, sizeof(Uint32));
    ScrollIndexes = (Uint8*)Memory::Calloc(ScrollIndexCount * 16, sizeof(Uint8));
}
PUBLIC bool SceneLayer::PropertyExists(char* property) {
    return Properties && Properties->Exists(property);
}
PUBLIC VMValue SceneLayer::PropertyGet(char* property) {
    if (!PropertyExists(property))
        return NULL_VAL;
    return Properties->Get(property);
}
PUBLIC void SceneLayer::SetTile(int x, int y, int tileID) {
    SetTile(x, y, tileID, false, false, TILE_COLLA_MASK, TILE_COLLB_MASK);
}
PUBLIC void SceneLayer::SetTile(int x, int y, int tileID, bool flip_x, bool flip_y) {
    SetTile(x, y, tileID, flip_x, flip_y, TILE_COLLA_MASK, TILE_COLLB_MASK);
}
PUBLIC void SceneLayer::SetTile(int x, int y, int tileID, bool flip_x, bool flip_y, int collA, int collB) {
    if (x < 0 || y < 0 || x >= Width || y >= Height)
        return;

    Uint32* tile = &Tiles[x + (y << WidthInBits)];

    *tile = tileID & TILE_IDENT_MASK;
    if (flip_x)
        *tile |= TILE_FLIPX_MASK;
    if (flip_y)
        *tile |= TILE_FLIPY_MASK;

    *tile |= collA;
    *tile |= collB;
}
PUBLIC void SceneLayer::SetTileCollisionSides(int x, int y, int collA, int collB) {
    if (x < 0 || y < 0 || x >= Width || y >= Height)
        return;

    collA <<= 28;
    collB <<= 26;

    Uint32* tile = &Tiles[x + (y << WidthInBits)];

    *tile &= TILE_FLIPX_MASK | TILE_FLIPY_MASK | TILE_IDENT_MASK;
    *tile |= collA;
    *tile |= collB;
}
PUBLIC void SceneLayer::SetCollidable(bool collidable) {
    if (collidable)
        Flags |=  SceneLayer::FLAGS_COLLIDEABLE;
    else
        Flags &= ~SceneLayer::FLAGS_COLLIDEABLE;
}
PUBLIC void SceneLayer::SetRepeatable(bool repeatableX, bool repeatableY) {
    if (repeatableX)
        Flags |=  SceneLayer::FLAGS_REPEAT_X;
    else
        Flags &= ~SceneLayer::FLAGS_REPEAT_X;

    if (repeatableY)
        Flags |=  SceneLayer::FLAGS_REPEAT_Y;
    else
        Flags &= ~SceneLayer::FLAGS_REPEAT_Y;
}
PUBLIC void SceneLayer::SetRepeatable(bool repeatable) {
    SetRepeatable(repeatable, repeatable);
}
PUBLIC void SceneLayer::SetInternalSize(int w, int h) {
    if (w > 0) {
        Width = w;
    }
    if (h > 0) {
        Height = h;
    }
}
PUBLIC void SceneLayer::SetScroll(float relative, float constant) {
    RelativeY = (short)(relative * 0x100);
    ConstantY = (short)(constant * 0x100);
}
PUBLIC void SceneLayer::SetTileDeforms(int lineIndex, int deformA, int deformB) {
    const int maxDeformLineMask = MAX_DEFORM_LINES - 1;

    lineIndex &= maxDeformLineMask;
    DeformSetA[lineIndex] = deformA;
    DeformSetB[lineIndex] = deformB;
}
PUBLIC void SceneLayer::SetCustomScanlineFunction(void* fn) {
    if (fn == nullptr) {
        UsingCustomScanlineFunction = false;
    }
    else {
        ObjFunction* function = (ObjFunction*)fn;
        CustomScanlineFunction = *function;
        UsingCustomScanlineFunction = true;
    }
}
PUBLIC void    SceneLayer::Dispose() {
    if (Properties)
        delete Properties;
    if (ScrollInfos)
        Memory::Free(ScrollInfos);
    if (ScrollInfosSplitIndexes)
        Memory::Free(ScrollInfosSplitIndexes);

    Memory::Free(Tiles);
    Memory::Free(TilesBackup);
    Memory::Free(ScrollIndexes);
}
