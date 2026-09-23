// Viewport 2.0 shade fragments for the oklchRamp node.
//
// Graph "oklchRamp":
//   oklchRampBase  : samples a baked 1D texture (RGB = colour, A = OKLab L) -> float4
//   oklchRampRGB   : float4 -> float3
//   oklchRampA     : float4 -> float
//   combineMayaTextureOutput / mayaTextureOutput : Maya built-ins -> outColor / outAlpha
#pragma once

namespace oklchFragments {

inline const char* kBaseName  = "oklchRampBase";
inline const char* kRGBName   = "oklchRampRGB";
inline const char* kAName     = "oklchRampA";
inline const char* kGraphName = "oklchRamp";

inline const char* kBaseXML = R"XML(
<fragment uiName="oklchRampBase" name="oklchRampBase" type="plumbing" class="ShadeFragment" version="1.0">
  <description><![CDATA[OKLCH ramp: samples a baked 1D ramp texture]]></description>
  <properties>
    <float2 name="uvCoord" semantic="mayaUvCoordSemantic" flags="varyingInputParam" />
    <int name="rampMode" />
    <float name="inputValue" />
    <texture2 name="map" />
    <sampler name="mapSampler" />
  </properties>
  <values>
    <int name="rampMode" value="0" />
    <float name="inputValue" value="0.0" />
  </values>
  <outputs>
    <float4 name="outColor" />
  </outputs>
  <implementation>
    <implementation render="OGSRenderer" language="Cg" lang_version="2.1">
      <function_name val="oklchRampBase" />
      <source><![CDATA[
float oklchRampPos(float2 uv, int mode, float inputValue)
{
    float du = uv.x - 0.5f, dv = uv.y - 0.5f;
    if (mode == 6) return inputValue;
    if (mode == 1) return uv.x;
    if (mode == 2) return 0.5f * (uv.x + uv.y);
    if (mode == 3) { float a = atan2(dv, du) / 6.28318530718f; return a < 0.0f ? a + 1.0f : a; }
    if (mode == 4) return 2.0f * sqrt(du * du + dv * dv);
    if (mode == 5) return 2.0f * max(abs(du), abs(dv));
    return uv.y;
}
float4 oklchRampBase(float2 uv, int rampMode, float inputValue, texture2D map, sampler2D mapSampler)
{
    float t = saturate(oklchRampPos(uv, rampMode, inputValue));
    return tex2D(mapSampler, float2(t, 0.5f));
}
]]></source>
    </implementation>
    <implementation render="OGSRenderer" language="HLSL" lang_version="11.0">
      <function_name val="oklchRampBase" />
      <source><![CDATA[
float oklchRampPos(float2 uv, int mode, float inputValue)
{
    float du = uv.x - 0.5f, dv = uv.y - 0.5f;
    if (mode == 6) return inputValue;
    if (mode == 1) return uv.x;
    if (mode == 2) return 0.5f * (uv.x + uv.y);
    if (mode == 3) { float a = atan2(dv, du) / 6.28318530718f; return a < 0.0f ? a + 1.0f : a; }
    if (mode == 4) return 2.0f * sqrt(du * du + dv * dv);
    if (mode == 5) return 2.0f * max(abs(du), abs(dv));
    return uv.y;
}
float4 oklchRampBase(float2 uv, int rampMode, float inputValue, Texture2D map, sampler mapSampler)
{
    float t = saturate(oklchRampPos(uv, rampMode, inputValue));
    return map.SampleLevel(mapSampler, float2(t, 0.5f), 0);
}
]]></source>
    </implementation>
    <implementation render="OGSRenderer" language="GLSL" lang_version="3.0">
      <function_name val="oklchRampBase" />
      <source><![CDATA[
float oklchRampPos(vec2 uv, int mode, float inputValue)
{
    float du = uv.x - 0.5, dv = uv.y - 0.5;
    if (mode == 6) return inputValue;
    if (mode == 1) return uv.x;
    if (mode == 2) return 0.5 * (uv.x + uv.y);
    if (mode == 3) { float a = atan(dv, du) / 6.28318530718; return a < 0.0 ? a + 1.0 : a; }
    if (mode == 4) return 2.0 * sqrt(du * du + dv * dv);
    if (mode == 5) return 2.0 * max(abs(du), abs(dv));
    return uv.y;
}
vec4 oklchRampBase(vec2 uv, int rampMode, float inputValue, sampler2D mapSampler)
{
    float t = clamp(oklchRampPos(uv, rampMode, inputValue), 0.0, 1.0);
    return textureLod(mapSampler, vec2(t, 0.5), 0.0);
}
]]></source>
    </implementation>
  </implementation>
</fragment>
)XML";

inline const char* kRGBXML = R"XML(
<fragment uiName="oklchRampRGB" name="oklchRampRGB" type="plumbing" class="ShadeFragment" version="1.0">
  <description><![CDATA[float4 -> float3]]></description>
  <properties>
    <float4 name="input" />
  </properties>
  <values>
    <float4 name="input" value="0.0,0.0,0.0,0.0" />
  </values>
  <outputs>
    <float3 name="output" />
  </outputs>
  <implementation>
    <implementation render="OGSRenderer" language="Cg" lang_version="2.1">
      <function_name val="oklchRampRGB" />
      <source><![CDATA[
float3 oklchRampRGB(float4 input) { return input.rgb; }
]]></source>
    </implementation>
    <implementation render="OGSRenderer" language="HLSL" lang_version="11.0">
      <function_name val="oklchRampRGB" />
      <source><![CDATA[
float3 oklchRampRGB(float4 input) { return input.rgb; }
]]></source>
    </implementation>
    <implementation render="OGSRenderer" language="GLSL" lang_version="3.0">
      <function_name val="oklchRampRGB" />
      <source><![CDATA[
vec3 oklchRampRGB(vec4 input) { return input.rgb; }
]]></source>
    </implementation>
  </implementation>
</fragment>
)XML";

inline const char* kAXML = R"XML(
<fragment uiName="oklchRampA" name="oklchRampA" type="plumbing" class="ShadeFragment" version="1.0">
  <description><![CDATA[float4 -> float (alpha)]]></description>
  <properties>
    <float4 name="input" />
  </properties>
  <values>
    <float4 name="input" value="0.0,0.0,0.0,0.0" />
  </values>
  <outputs>
    <float name="output" />
  </outputs>
  <implementation>
    <implementation render="OGSRenderer" language="Cg" lang_version="2.1">
      <function_name val="oklchRampA" />
      <source><![CDATA[
float oklchRampA(float4 input) { return input.a; }
]]></source>
    </implementation>
    <implementation render="OGSRenderer" language="HLSL" lang_version="11.0">
      <function_name val="oklchRampA" />
      <source><![CDATA[
float oklchRampA(float4 input) { return input.a; }
]]></source>
    </implementation>
    <implementation render="OGSRenderer" language="GLSL" lang_version="3.0">
      <function_name val="oklchRampA" />
      <source><![CDATA[
float oklchRampA(vec4 input) { return input.a; }
]]></source>
    </implementation>
  </implementation>
</fragment>
)XML";

inline const char* kGraphXML = R"XML(
<fragment_graph name="oklchRamp" ref="oklchRamp" class="FragmentGraph" version="1.0">
  <fragments>
    <fragment_ref name="mayaTextureOutput" ref="mayaTextureOutput" />
    <fragment_ref name="combineMayaTextureOutput" ref="combineMayaTextureOutput" />
    <fragment_ref name="oklchRampBase" ref="oklchRampBase" />
    <fragment_ref name="oklchRampRGB" ref="oklchRampRGB" />
    <fragment_ref name="oklchRampA" ref="oklchRampA" />
  </fragments>
  <connections>
    <connect from="combineMayaTextureOutput.output" to="mayaTextureOutput.mayaTextureOutput" name="mayaTextureOutput" />
    <connect from="oklchRampRGB.output" to="combineMayaTextureOutput.base" name="base" />
    <connect from="oklchRampA.output" to="combineMayaTextureOutput.alpha" name="alpha" />
    <connect from="oklchRampBase.outColor" to="oklchRampRGB.input" name="input" />
    <connect from="oklchRampBase.outColor" to="oklchRampA.input" name="input" />
  </connections>
  <properties>
    <float2 name="uvCoord" ref="oklchRampBase.uvCoord" semantic="mayaUvCoordSemantic" flags="varyingInputParam" />
    <int name="rampMode" ref="oklchRampBase.rampMode" />
    <float name="inputValue" ref="oklchRampBase.inputValue" />
    <texture2 name="map" ref="oklchRampBase.map" />
    <sampler name="mapSampler" ref="oklchRampBase.mapSampler" />
  </properties>
  <values>
    <int name="rampMode" value="0" />
    <float name="inputValue" value="0.0" />
  </values>
  <outputs>
    <struct name="mayaTextureOutput" ref="mayaTextureOutput.mayaTextureOutput" />
  </outputs>
</fragment_graph>
)XML";

} // namespace oklchFragments
