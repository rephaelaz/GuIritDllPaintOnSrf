How to use the GuIrit Paint on Surface DLL:
- The DLL allows to set a texture over a surface and to draw on the texture.
- User has to select a texture from the 'Surface' menu. Upon selection, a
  texture will be created for the selected surface with default dimensions and
  color.
- User can draw on the selected surface by pressing Ctrl while clicking on the
  surface.

DLL Panel Description:
- The 'Load Texture' button can be used to load a PNG file and apply it to the
  selected surface. This will replace the current texture of the selected 
  surface.
- The 'Save texture' button allows to save the current texture of the selected
  surface as a PNG file.
- The 'Reset texture' will set the current texture of the selected surface to a
  white 256x256 texture.
- Texture Width and Texture Height allow to change the size of the texture.
- Color will display a color selection menu to change the color of the shape
  drawn on the selected surface.
- Alpha controls the transparency of the shape.
- Shape will display a menu listing all the possible shapes to use to draw on
  the surface. User can edit or add shapes by putting them as PNG files in the
  directory "GuIritData\Auxiliary\SandArt\Masks". Each shape should be
  represented as a grayscale picture, where black will be transparent and white
  will be opaque.
- X Factor and Y Factor allow to control the width and the height of the shape
  respectively.