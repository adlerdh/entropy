All color maps in this directory are derived from the Library of Perceptually Uniform Colour Maps.

The newer CSVs (C03, C06-C11, C01s-C04s, C06s-C07s, D05, L13-L15,
L20, R04, CBL03-CBL04, CBD02, and CBTL03-CBTL04) were generated from
Peter Kovesi's six-decimal colorcet.m arrays dated 19-Dec-2020:
https://www.peterkovesi.com/matlabfns/Colourmaps/colorcet.m
The new maps use short category-first display names and descriptive
tooltips, following the style of the older maps. The names and tooltips
of the older maps are unchanged.
The shifted cyclic maps are rotations by one quarter of a cycle, matching
the downloadable C1s-C7s CSVs where available. C05s was omitted because
it is grayscale, as were the L01 and L02 grayscale maps. The older C05
cyclic grayscale map was already bundled and has not been changed.
Existing maps, including the older L16 design, have not been replaced.

For more information about the design of these colour maps see:
Peter Kovesi. Good Colour Maps: How to Design Them.
arXiv:1509.03700 [cs.GR] 2015
https://arxiv.org/abs/1509.03700

See also:
https://colorcet.holoviz.org/

Copyright (c) 2014-2018 Peter Kovesi
Centre for Exploration Targeting
The University of Western Australia
peter.kovesi@uwa.edu.au

These colour maps are released under the Creative Commons BY License.
A summary of the conditions can be found at
https://creativecommons.org/licenses/by/4.0/

The following text is copied verbatim from Matlab code posted by Peter Kovesi at
https://colorcet.holoviz.org/


Colour Map naming convention:

                   linear_kryw_5-100_c67_n256
                     /      /    |    \    \
 Colour Map attribute(s)   /     |     \   Number of colour map entries
                          /      |      \
    String indicating nominal    |      Mean chroma of colour map
    hue sequence.                |
                             Range of lightness values

In addition, the name of the colour map may have cyclic shift information
appended to it, it may also have a flag indicating it is reversed. 
                                             
             cyclic_wrwbw_90-40_c42_n256_s25_r
                                         /    \
                                        /   Indicates that the map is reversed.
                                       / 
                 Percentage of colour map length
                 that the map has been rotated by.

* Attributes may be: linear, diverging, cyclic, rainbow, or isoluminant.  A
  colour map may have more than one attribute. For example, diverging-linear or
  cyclic-isoluminant.

* Lightness values can range from 0 to 100. For linear colour maps the two
  lightness values indicate the first and last lightness values in the
  map. For diverging colour maps the second value indicates the lightness value
  of the centre point of the colour map (unless it is a diverging-linear
  colour map). For cyclic and rainbow colour maps the two values indicate the
  minimum and maximum lightness values. Isoluminant colour maps have only
  one lightness value. 

* The string of characters indicating the nominal hue sequence uses the following code
     r - red      g - green      b - blue
     c - cyan     m - magenta    y - yellow
     o - orange   v - violet 
     k - black    w - white      j - grey

  ('j' rhymes with grey). Thus a 'heat' style colour map would be indicated by
  the string 'kryw'. If the colour map is predominantly one colour then the
  full name of that colour may be used. Note these codes are mainly used to
  indicate the hues of the colour map independent of the lightness/darkness and
  saturation of the colours.

* Mean chroma/saturation is an indication of vividness of the colour map. A
  value of 0 corresponds to a greyscale. A value of 50 or more will indicate a
  vivid colour map.
