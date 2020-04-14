
inline bool32
InteractionsAreEqual(debug_interaction A, debug_interaction B)
{
    bool32 Result = (DebugIDsAreEqual(A.ID, B.ID) &&
                     (A.Target == B.Target) &&
                     (A.Type == B.Type) &&
                     (A.Generic == B.Generic));
    return(Result);
}

inline bool32
InteractionIsHot(debug_state *DebugState, debug_interaction B)
{
    bool32 Result = InteractionsAreEqual(DebugState->HotInteraction, B);

    if(B.Type == DebugInteraction_None)
    {
        Result = false;
    }

    return(Result);
}

inline bool32
IsHex(char Char)
{
    bool32 Result = (((Char >= '0') && (Char <= '9')) ||
                    ((Char >= 'A') && (Char <= 'F')));
    return(Result);
}

inline uint32
GetHex(char Char)
{
    uint32 Result = 0;

    if((Char >= '0') && (Char <= '9'))
    {
        Result = Char - '0';
    }
    else if((Char >= 'A') && (Char <= 'F'))
    {
        Result = 0xA + (Char - 'A');
    }

    return(Result);
}

internal rectangle2
TextOp(debug_state *DebugState, debug_text_op Op, v2 P, char *String, v4 Color = V4(1, 1, 1, 1),
       r32 AtZ = 0.0f)
{
    rectangle2 Result = InvertedInfinityRectangle2();
    if(DebugState && DebugState->DebugFont)
    {
        render_group *RenderGroup = &DebugState->RenderGroup;
        loaded_font *Font = DebugState->DebugFont; 
        hha_font *FontInfo = DebugState->DebugFontInfo;

        uint32 PrevCodePoint = 0;
        real32 AtX = P.x;
        real32 AtY = P.y;
        for(char *At = String;
            *At;
            At++)
        {
            uint32 CodePoint = *At;
            if((At[0] == '\\') &&
            (IsHex(At[1])) && 
            (IsHex(At[2])) &&
            (IsHex(At[3])) &&
            (IsHex(At[4])))
            {
                CodePoint = ((GetHex(At[1]) << 12) |
                            (GetHex(At[2]) << 8) |
                            (GetHex(At[3]) << 4) |
                            (GetHex(At[4]) << 0));
                            
                At += 4;
            }

            real32 AdvanceX = DebugState->FontScale*GetHorizontalAdvanceForPair(FontInfo, Font, PrevCodePoint, CodePoint);
            AtX += AdvanceX;

            if(*At != ' ')
            {
                bitmap_id BitmapID = GetBitmapForGlyph(RenderGroup->Assets, FontInfo, Font, CodePoint);
                hha_bitmap *Info = GetBitmapInfo(RenderGroup->Assets, BitmapID);

                real32 BitmapScale = DebugState->FontScale*Info->Dim[1];
                v3 BitmapOffset = V3(AtX, AtY, AtZ);
                if(Op == DEBUGTextOp_DrawText)
                {
                    PushBitmap(RenderGroup, DebugState->TextTransform, BitmapID, BitmapScale, BitmapOffset, Color, 1.0f);
                    PushBitmap(RenderGroup, DebugState->ShadowTransform, BitmapID, BitmapScale, 
                        BitmapOffset + V3(2.0f, -2.0f, 0.0f), V4(0, 0, 0, 1.0f), 1.0f);
                }
                else
                {
                    Assert(Op == DEBUGTextOp_SizeText);

                    loaded_bitmap *Bitmap = GetBitmap(RenderGroup->Assets, BitmapID, RenderGroup->GenerationID);
                    if(Bitmap)
                    {
                        used_bitmap_dim Dim = GetBitmapDim(RenderGroup, DefaultFlatTransform(), Bitmap, BitmapScale, BitmapOffset, 1.0f);
                        rectangle2 GlyphDim = RectMinDim(Dim.P.xy, Dim.Size);
                        Result = Union2(Result, GlyphDim);
                    }
                }
            }

            PrevCodePoint = CodePoint;
        }
    }

    return(Result);
}

inline void
TextOutAt(debug_state *DebugState, v2 P, char *String, v4 Color = V4(1, 1, 1, 1), r32 AtZ = 0.0f)
{
    if(DebugState)
    {
        TextOp(DebugState, DEBUGTextOp_DrawText, P, String, Color, AtZ); 
    }
}

inline rectangle2
GetTextSize(debug_state *DebugState, char *String)
{
    rectangle2 Result = TextOp(DebugState, DEBUGTextOp_SizeText, V2(0, 0), String);
    
    return(Result);
}

inline r32
GetLineAdvance(debug_state *DebugState)
{
    r32 Result = GetLineAdvanceFor(DebugState->DebugFontInfo)*DebugState->FontScale;
    return(Result);
}

inline r32
GetBaseline(debug_state *DebugState)
{
    r32 Result = DebugState->FontScale*GetStartingBaselineY(DebugState->DebugFontInfo);
    return(Result);
}

inline debug_interaction
SetPointerInteraction(debug_id DebugID, void **Target, void *Value)
{
    debug_interaction Result = {};

    Result.ID = DebugID;
    Result.Type = DebugInteraction_SetPointer;
    Result.Target = Target;
    Result.Pointer = Value;

    return(Result);
}

inline debug_interaction
SetUInt32Interaction(debug_id DebugID, u32 *Target, u32 Value)
{
    debug_interaction Result = {};

    Result.ID = DebugID;
    Result.Type = DebugInteraction_SetUInt32;
    Result.Target = Target;
    Result.UInt32 = Value;

    return(Result);
}

inline debug_interaction
ElementInteraction(debug_state *DebugState, debug_id DebugID, debug_interaction_type Type, debug_element *Element)
{
    debug_interaction Result = {};
    Result.ID = DebugID;
    Result.Type = Type;
    Result.Element = Element;

    return(Result);
}

inline debug_interaction
DebugIDInteraction(debug_interaction_type Type, debug_id ID)
{
    debug_interaction Result = {};
    Result.ID = ID;
    Result.Type = Type;

    return(Result);
}

inline debug_interaction
DebugLinkInteraction(debug_interaction_type Type, debug_variable_link *Link)
{
    debug_interaction Result = {};
    Result.Link = Link;
    Result.Type = Type;

    return(Result);
}

inline layout
BeginLayout(debug_state *DebugState, v2 MouseP, v2 UpperLeftCorner)
{
    layout Layout = {};

    Layout.DebugState = DebugState;
    Layout.MouseP = MouseP;
    Layout.BaseCorner = Layout.At = UpperLeftCorner;
    Layout.LineAdvance = GetLineAdvanceFor(DebugState->DebugFontInfo)*DebugState->FontScale;
    Layout.SpacingX = 4.0f;
    Layout.SpacingY = 4.0f;

    return(Layout);
}

inline void
EndLayout(layout *Layout)
{

}

inline layout_element 
BeginElementRectangle(layout *Layout, v2 *Dim)
{
    layout_element Element = {};

    Element.Layout = Layout;
    Element.Dim = Dim;

    return(Element);
}

inline void    
MakeElementSizeable(layout_element *Element)
{
    Element->Size = Element->Dim;
}

inline void
DefaultInteraction(layout_element *Element, debug_interaction Interaction)
{
    Element->Interaction = Interaction;
}

inline void
AdvanceElement(layout *Layout, rectangle2 ElRect)
{
    Layout->NextYDelta = Minimum(Layout->NextYDelta, GetMinCorner(ElRect).y - Layout->At.y);

    if(Layout->NoLineFeed)
    {
        Layout->At.x = GetMaxCorner(ElRect).x + Layout->SpacingX;
    }
    else
    {
        Layout->At.y += Layout->NextYDelta - Layout->SpacingY;
        Layout->LineInitialized = false;
    }
}

inline void
EndElement(layout_element *Element)
{
    layout *Layout = Element->Layout;
    debug_state *DebugState = Layout->DebugState;
    object_transform NoTransform = DebugState->BackingTransform;

    if(!Layout->LineInitialized)
    {
        Layout->At.x = Layout->BaseCorner.x + Layout->Depth*2.0f*Layout->LineAdvance;
        Layout->NextYDelta = 0.0f;

        Layout->LineInitialized = true;
    }

    real32 SizeHandlePixels = 4.0f;

    v2 Frame = {};
    if(Element->Size)
    {
        Frame = V2(SizeHandlePixels, SizeHandlePixels);
    }
    v2 TotalDim = *Element->Dim + 2.0f*Frame;

    v2 TotalMinCorner = V2(Layout->At.x, 
                           Layout->At.y - TotalDim.y);
    v2 TotalMaxCorner = TotalMinCorner + TotalDim;

    v2 InteriorMinCorner = TotalMinCorner + Frame;
    v2 InteriorMaxCorner = InteriorMinCorner + *Element->Dim;

    rectangle2 TotalBounds = RectMinMax(TotalMinCorner, TotalMaxCorner);
    Element->Bounds = RectMinMax(InteriorMinCorner, InteriorMaxCorner);

    if(Element->Interaction.Type && IsInRectangle(Element->Bounds, Layout->MouseP))
    {
        DebugState->NextHotInteraction = Element->Interaction;
    }

    if(Element->Size)
    {
        PushRect(&DebugState->RenderGroup, NoTransform, RectMinMax(V2(TotalMinCorner.x, InteriorMinCorner.y), 
                                                                   V2(InteriorMinCorner.x, InteriorMaxCorner.y)), 
                0.0f, V4(0, 0, 0, 1));
        PushRect(&DebugState->RenderGroup, NoTransform, RectMinMax(V2(InteriorMaxCorner.x, InteriorMinCorner.y), 
                                                                   V2(TotalMaxCorner.x, InteriorMaxCorner.y)), 
                0.0f, V4(0, 0, 0, 1));
        PushRect(&DebugState->RenderGroup, NoTransform, RectMinMax(V2(InteriorMinCorner.x, InteriorMaxCorner.y), 
                                                                   V2(InteriorMaxCorner.x, TotalMaxCorner.y)), 
                0.0f, V4(0, 0, 0, 1));
        PushRect(&DebugState->RenderGroup, NoTransform, RectMinMax(V2(InteriorMinCorner.x, TotalMinCorner.y), 
                                                                   V2(InteriorMaxCorner.x, InteriorMinCorner.y)), 
                0.0f, V4(0, 0, 0, 1));

        debug_interaction SizeInteraction = {};
        SizeInteraction.Type = DebugInteraction_Resize;
        SizeInteraction.P = Element->Size;

        rectangle2 SizeBox = AddRadiusTo(RectMinMax(V2(InteriorMaxCorner.x, TotalMinCorner.y),
                                                    V2(TotalMaxCorner.x, InteriorMinCorner.y)), 
                                         V2(4.0f, 4.0f));
        PushRect(&DebugState->RenderGroup, NoTransform, SizeBox, 0.0f, 
                 InteractionIsHot(DebugState, SizeInteraction) ? 
                 V4(1, 1, 0, 1) : V4(1, 1, 1, 1));
        
        if(IsInRectangle(SizeBox, Layout->MouseP))
        {
            DebugState->NextHotInteraction = SizeInteraction;
        }
    }

    AdvanceElement(Layout, TotalBounds);
}

internal v2
BasicTextElement(layout *Layout, char *Text, debug_interaction ItemInteraction,
                 v4 ItemColor = V4(0.8f, 0.8f, 0.8f, 1), v4 HotColor = V4(1, 1, 1, 1),
                 r32 Border = 0.0f, v4 BackdropColor = V4(0, 0, 0, 0))
{
    debug_state *DebugState = Layout->DebugState;

    rectangle2 TextBounds = GetTextSize(DebugState, Text);
    v2 Dim = {GetDim(TextBounds).x + 2.0f*Border, Layout->LineAdvance + 2.0f*Border};

    layout_element Element = BeginElementRectangle(Layout, &Dim);
    DefaultInteraction(&Element, ItemInteraction);
    EndElement(&Element);

    b32 IsHot = InteractionIsHot(Layout->DebugState, ItemInteraction);

    TextOutAt(DebugState, V2(GetMinCorner(Element.Bounds).x + Border, 
                             GetMaxCorner(Element.Bounds).y - Border - DebugState->FontScale*GetStartingBaselineY(DebugState->DebugFontInfo)), 
              Text, IsHot ? HotColor : ItemColor);

    if(BackdropColor.w > 0.0f)
    {
        PushRect(&DebugState->RenderGroup, DebugState->BackingTransform, Element.Bounds, 0.0f, BackdropColor);
    }

    return(Dim);
}

internal void
BeginRow(layout *Layout)
{
    Layout->NoLineFeed++;
}

internal void 
Label(layout *Layout, char *Name)
{
    debug_interaction NullInteraction = {};
    BasicTextElement(Layout, Name, NullInteraction, V4(1, 1, 1, 1.0f), V4(1, 1, 1, 1));
}

internal void 
ActionButton(layout *Layout, char *Name, debug_interaction Interaction)
{
    BasicTextElement(Layout, Name, Interaction, 
                     V4(0.5f, 0.5f, 0.5f, 1.0f), V4(1, 1, 1, 1), 
                     4.0f, V4(0, 0.5f, 1.0f, 1.0f));
}

internal void 
BooleanButton(layout *Layout, char *Name, b32 Highlight, debug_interaction Interaction)
{
    BasicTextElement(Layout, Name, Interaction, 
                     Highlight ? V4(1, 1, 1, 1) : V4(0.5f, 0.5f, 0.5f, 1.0f),
                     V4(1, 1, 1, 1), 4.0f, V4(0, 0.5f, 1.0f, 1.0f));
}

internal void
EndRow(layout *Layout)
{
    Assert(Layout->NoLineFeed > 0 );
    Layout->NoLineFeed--;

    AdvanceElement(Layout, RectMinMax(Layout->At, Layout->At));
}

internal void
AddTooltip(debug_state *DebugState, char *Text)
{
    render_group *RenderGroup = &DebugState->RenderGroup;
    u32 OldClipRect = RenderGroup->CurrentClipRectIndex;
    RenderGroup->CurrentClipRectIndex = DebugState->DefaultClipRect;
 
    layout *Layout = &DebugState->MouseTextLayout;

    rectangle2 TextBounds = GetTextSize(DebugState, Text);
    v2 Dim = {GetDim(TextBounds).x, Layout->LineAdvance};

    layout_element Element = BeginElementRectangle(Layout, &Dim);
    EndElement(&Element);

    TextOutAt(DebugState, V2(GetMinCorner(Element.Bounds).x, 
                             GetMaxCorner(Element.Bounds).y - DebugState->FontScale*GetStartingBaselineY(DebugState->DebugFontInfo)), 
              Text, V4(1, 1, 1, 1), 10000.0f);

    RenderGroup->CurrentClipRectIndex = OldClipRect;
}