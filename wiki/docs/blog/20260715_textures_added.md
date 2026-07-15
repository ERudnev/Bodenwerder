# Текстуры добавлены

2026-07-15

В `rmmr` добавлен первый полноценный texture pipeline. Входной точкой был драфт `resources/texture.q1.types`. Дальше работа пошла по двум зафиксированным условиям:

- старый `lit` material остаётся без изменений;
- textured path делается отдельным material preset;
- texture binding хранится по варианту B: texture baked в material;
- `asset::Texture` использует `library`, как `asset::Shader`.

## Q1

Добавлен `asset::Texture`:

```q1
entity Texture
  always
    ?filename(name: string, library: string) -> string
  one
    name: string
    library: string
    >compile(#Device) -> #resource::Texture
```

`resource::Texture` оставлен минимальным:

```q1
entity Texture
  using Handle as @external(OpenGL C++ texture handle)
  one
    handle: Handle
    size: index2

group<Texture> of Device
```

В `asset::Material` и `resource::Material` добавлен `TextureBinding`:

```q1
struct TextureBinding
  id: Uniform::Id
  texture: #asset::Texture
```

```q1
struct TextureBinding
  id: Uniform::Id
  texture: #Texture
```

Также `asset::Geometry` расширен полем `uv0`:

```q1
positions: vector<Pos>
normals: vector<Pos>
uv0: vector<UV>
indices: vector<integer>
```

## Compile path

Добавлен `asset::Texture::compile(...)`:

```cpp
const auto path = resolve_under_asset_root(
    asset_root,
    Texture::Always::filename(asset.name, asset.library));

stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);
glGenTextures(1, &handle);
glBindTexture(GL_TEXTURE_2D, handle);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
glGenerateMipmap(GL_TEXTURE_2D);
```

Texture компилируется в `resource::Texture_group` на `Device`.

В `asset::Material::compile(...)` added texture bake:

```cpp
for (const auto& texture_binding : asset.textures) {
    textures.push_back(resource::Material::TextureBinding{
        .id = texture_binding.id,
        .texture = asset::Texture::Actions::compile(context, texture_binding.texture, device),
    });
}
```

Это и есть реализация варианта B: material владеет уже скомпилированными texture resources.

## Новый material path

В vocabulary добавлен новый semantic:

```cpp
Entry{109, Type::sampler2d, "shadowMap"},
Entry{110, Type::sampler2d, "albedoMap"},
```

Добавлен новый preset:

```cpp
auto MaterialGenerator::litTextured(
    Writing context,
    system::Device::Id device,
    asset::Texture::Id albedo_map) -> resource::Material::Id
```

Он использует palette:

```cpp
{
    "model",
    "view",
    "projection",
    "albedo",
    "albedoMap",
    "ambientColor",
    "ambientIntensity",
    "light0Pos",
    "light0Color",
    "light0Intensity",
    "lightSpaceMatrix",
    "shadowMap",
}
```

Старый `lit` material не менялся.

## Shader и geometry

Для нового пути добавлена пара `litTextured.vert.glsl` / `litTextured.frag.glsl`.

Vertex shader получил `aUv0`:

```glsl
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUv0;
```

Fragment shader берёт base color из texture:

```glsl
uniform vec3 u_albedo;
uniform sampler2D u_albedoMap;

vec3 baseColor = texture(u_albedoMap, v_uv0).rgb * u_albedo;
```

В `asset::Geometry::compile(...)` добавлен третий layout:

```cpp
const bool position_normal_uv0 =
    asset.layout.size() == std::size_t{3}
    && asset.layout[0] == pos_id
    && asset.layout[1] == normal_id
    && asset.layout[2] == uv0_id;
```

`GeometryGenerator::kube()` переведён на:

```cpp
{"position", "normal", "uv0"}
```

и получил UV для всех граней.

## Renderer

`Renderer` теперь умеет доставать texture из material runtime data:

```cpp
} else if (name == "albedoMap") {
    const auto texture = material_texture_for_semantic(material_quantum, binding.id);
    set_uniform_sampler(binding, with<resource::Texture>::get(args.world, *texture).handle, 0);
}
```

`shadowMap` при этом остаётся отдельным infrastructural binding.

## Engine

В `Engine::prepareResources()` добавлен один hardcoded asset:

```cpp
const auto touched_texture = with<asset::Texture>::create(main, asset::Texture::Quantum{
    .name = "touched.jfif",
    .library = "rmmr",
});
```

Из него собирается новый material:

```cpp
state->resources.materialLitTextured =
    material::MaterialGenerator::litTextured(main, window, touched_texture);
```

Кубы сцены переведены на `materialLitTextured`.

## Результат

Получился рабочий end-to-end путь:

`assets/rmmr/textures/touched.jfif`
→ `asset::Texture`
→ `resource::Texture`
→ `resource::Material(textures=...)`
→ `Renderer`

Проверка:

- `Raidenmamare` собирается;
- `Toy` собирается;
- общий build дерева по-прежнему падает на уже существующей ошибке в `modules/base.cpp/test/serialization_roundtrip.cpp`, не связанной с texture work.
