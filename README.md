# BSDM
Using Bi-Sectional Diffusion Model to build an encoder grid-based hit-map utilizing the furthest difference RGB determiner (background-color)


## Problems
- This method at the moment doesn't consider the previous color but does consider the previous value and at the moment, the value isn't directly represented by the color, so the color at the moment is to help show which of the hit points were most likely hit since it'll have the biggest difference value amoungst it's closest phyiscally touching colors. This ensures there is no gurentee the hit-map can be used to directly unwrap the encorder but could be potentially used to ensure it's always going in the correct direction during encoding to make sure the decoding wrapping process works correctly (ref to https://github.com/DigiMancer3D/SuPosXt [SuPosXt]).

- This method isn't stacking hits but the code is setup for hit stacking (to get the diffused coloring around hits, which when hit-stacking is added, should allow for high-hits and low-hits amoung entropy-shading.

- This method doesn't correct misfires & misdirected-overflows

- Bridging Map Output happens too often (shows lack of randomization in maths, may need randomizing-directory statments)
  

## Not-Problems
- The numerical values do work as intentded to show hit and hits stack

- "Always Up" redirecting helps ensure correct hits

- Endput mapping works properly

- user hit map works properly


