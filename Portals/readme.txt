-Added a specular map that gets sent to the pixel shader. The specular map then gets sampled (just a single channel), and scales the specular calucation by
said value (0 - 1).