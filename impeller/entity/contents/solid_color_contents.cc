// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "solid_color_contents.h"

#include "impeller/entity/contents/content_context.h"
#include "impeller/entity/entity.h"
#include "impeller/geometry/path.h"
#include "impeller/geometry/path_builder.h"
#include "impeller/renderer/render_pass.h"
#include "impeller/tessellator/tessellator.h"

namespace impeller {

SolidColorContents::SolidColorContents() = default;

SolidColorContents::~SolidColorContents() = default;

void SolidColorContents::SetColor(Color color) {
  color_ = color;
}

const Color& SolidColorContents::GetColor() const {
  return color_;
}

void SolidColorContents::SetPath(Path path) {
  path_ = std::move(path);
}

void SolidColorContents::SetCover(bool cover) {
  cover_ = cover;
}

std::optional<Rect> SolidColorContents::GetCoverage(
    const Entity& entity) const {
  if (color_.IsTransparent()) {
    return std::nullopt;
  }
  return path_.GetTransformedBoundingBox(entity.GetTransformation());
};

bool SolidColorContents::ShouldRender(const Entity& entity,
                                      const ISize& target_size) const {
  return cover_ || Contents::ShouldRender(entity, target_size);
}

VertexBuffer SolidColorContents::CreateSolidFillVertices(const Path& path,
                                                         HostBuffer& buffer) {
  using VS = SolidFillPipeline::VertexShader;

  VertexBufferBuilder<VS::PerVertexData> vtx_builder;

  auto tesselation_result = Tessellator{}.Tessellate(
      path.GetFillType(), path.CreatePolyline(),
      [&vtx_builder](auto point) { vtx_builder.AppendVertex({point}); });
  if (tesselation_result != Tessellator::Result::kSuccess) {
    return {};
  }

  return vtx_builder.CreateVertexBuffer(buffer);
}

bool SolidColorContents::Render(const ContentContext& renderer,
                                const Entity& entity,
                                RenderPass& pass) const {
  using VS = SolidFillPipeline::VertexShader;
  using FS = SolidFillPipeline::FragmentShader;

  Command cmd;
  cmd.label = "Solid Fill";
  cmd.pipeline =
      renderer.GetSolidFillPipeline(OptionsFromPassAndEntity(pass, entity));
  cmd.stencil_reference = entity.GetStencilDepth();

  cmd.BindVertices(CreateSolidFillVertices(
      cover_
          ? PathBuilder{}.AddRect(Size(pass.GetRenderTargetSize())).TakePath()
          : path_,
      pass.GetTransientsBuffer()));

  VS::VertInfo vert_info;
  vert_info.mvp = Matrix::MakeOrthographic(pass.GetRenderTargetSize()) *
                  entity.GetTransformation();
  VS::BindVertInfo(cmd, pass.GetTransientsBuffer().EmplaceUniform(vert_info));

  FS::FragInfo frag_info;
  frag_info.color = color_.Premultiply();
  FS::BindFragInfo(cmd, pass.GetTransientsBuffer().EmplaceUniform(frag_info));

  cmd.primitive_type = PrimitiveType::kTriangle;

  if (!pass.AddCommand(std::move(cmd))) {
    return false;
  }

  return true;
}

std::unique_ptr<SolidColorContents> SolidColorContents::Make(Path path,
                                                             Color color) {
  auto contents = std::make_unique<SolidColorContents>();
  contents->SetPath(std::move(path));
  contents->SetColor(color);
  return contents;
}

}  // namespace impeller
