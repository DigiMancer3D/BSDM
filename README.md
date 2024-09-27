# BSDM
Using Bi-Sectional Diffusion Model to build an encoder grid-based hit-map utilizing the furthest difference RGB determiner (background-color)


## Problems
This method at the moment doesn't consider the previous color but does consider the previous value and at the moment, the value isn't directly represented by the color, so the color at the moment is to help show which of the hit points were most likely hit since it'll have the biggest difference value amoungst it's closest phyiscally touching colors. This ensures there is not gurentee the hit-map can be used to directly unwrap the encorder but could be potentially used to ensure it's always going in the correct direction during encoding to make sure the decoding wrapping process works correctly (ref to https://github.com/DigiMancer3D/SuPosXt [SuPosXt]).
