# CPLVK (Vulkan test project)
## About

This is a project I worked at for quite a time to explore another geaphics API besides of OpenGL. My attempt with the Vulkan API the newer versions in that case 1.4 where I succeeded a small abstraction which are similar like from my OpenGL gamedev library [CPL](https://github.com/ColinIndieDev/CPL).

## Example code

In this example, a blue square on the top left corner will be drawn on a black background
with Vulkan.

```c
#include "../cplvk/cplvk.h

int main(void) {
    enable_vsync(false);
    window_init(800, 800, "Hello world!");

    clear_background(BLACK);
    while (!window_should_close()) {
        begin_draw(DRAW_MODE_2D);

        draw_rect(VEC2F(0, 0), VEC2F(100, 100), RED, 0);
        
        end_draw();
    }
    window_close();
}
```

## Dependencies

- GLFW: window creation and inputs
- Vulkan (1.4): the graphics API
- VMA: Allocator
- Volk: provides the Vulkan functions etc. (like glad in OpenGL kinda)
- shaderc: glsl shader code compiler
- cpstd: my own utilities I use here too

## Conclusion

Even though it was interesting exploring Vulkan and its modern fancy features which honestly get deprecated quickly since it keeps developing very fast. And yeah in my example code for a scene it was indeed faster than OpenGL and it can be multithreaded too but since the API is very low level where everything has to be stup and managed manually.

In my opinion the debugging pain although the logs are more clearer than in OpenGL as well as the complexity and code to wrote oh and readability do make it very challenging so that I would stick with OpenGL and someday maybe Vulkan when I mastered graphics programming itself. I still have not discovered the entire 3D possible neither PBR or Raytracing where stuff goes very very interesting. 

And since I discovered for myself the newer OpenGL versions, I will keep using GL since it modern frwtures makes programming OpenGL „easier“ and kinda modern. With its version 4.5, I am able to use SSBOs like in Vulkan and even Compute is possible now which is tbh the only reason I tried out vulkan. On the other OpenGL is ofc still kinda slow because of its abstraction and is still bound to one single thread owning the context whereas vulkan does not have this problem at all. 

Either way I think OpenGL is still great especially if you start messing with its newer version rather than 3.3
