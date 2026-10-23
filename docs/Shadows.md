# Native GPU box shadows

`CanvasLayer::shadow` accepts a `BoxShadow` with logical bounds, corner radius,
Gaussian blur radius and linear-light sRGB color. It draws before that layer's
backdrop, panels and foreground. The bounds describe the shadow shape itself;
an owner applies an offset by moving those bounds. Ordinary layer clipping and
the viewport clip the result, including its transparent blur padding.

The native Metal path draws a white rounded coverage mask into a private R8
texture, blurs it into R16Float through the system MPS Gaussian kernel, and
composes the colored alpha on the GPU. The padded quad covers only the shadow's
region. Rendered shadows do not require CPU pixel generation or readback.
The kernel sigma is blur_radius/3. Zero blur keeps the sharp antialiased mask.
Positive blur is bounded to 64 logical units; padded physical dimensions must
remain within 8192 pixels per axis.

Mask and blur textures persist. Width, height, corner radius, blur radius and
backing scale determine the mask key. Moving a shape, changing its color or
changing the scene behind it reuses the same mask. A geometry change redraws
and reblurs it; removing the shadow releases its retained working textures.
The working mask uses one byte per pixel and an optional two-byte blur target.
The combined retained shadow and backdrop working-texture budget is 64 MiB.
Driver resources, ordinary attachments, foreground textures and in-flight
command resources remain outside this accounting.

`GraphicsInfo::shadow_supported` is explicit capability information.
`shadow_masks`, `shadow_blurs` and `shadow_texture_bytes` report encoded passes
and retained storage, not elapsed GPU time or physical scanout. Pipeline states
are created only when a shadow is first requested and then reused. Vulkan and
D3D11 shadows remain unimplemented; a request fails explicitly without dropping
the effect or changing it into an opaque rectangle.

`PopoverOptions::shadow` supplies an optional `PopoverShadow` with offset,
blur radius and color. It is absent by default. Widget composition emits its
shadow after dimming and before the card surface, so LVRS Alert can use the
authored downward 16-unit offset, 32-unit blur and 28% black color. Popup lifetime
and input policies remain owned by the same C++ tree.

`tests/shadows.cpp` checks validation, actual soft pixels, linear tint,
geometry-keyed reuse, release, budget exhaustion before mask allocation and an
unsupported backend. The Alert test additionally checks its actual downward
shadow and removal through `Alert::set_shadow_enabled`.
