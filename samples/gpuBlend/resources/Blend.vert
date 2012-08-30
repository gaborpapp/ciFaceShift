#version 120
#extension GL_EXT_gpu_shader4 : enable

attribute float index;

uniform sampler2DRect blendshapes;
uniform int numBlendshapes;
uniform float blend;

void main()
{
//*
	vec2 uv = vec2( ( ( int( index ) & 31 ) * numBlendshapes ) + 21. + .5,
					( int( index ) / 32 ) + .5 );
//*/
/*
	vec2 uv = vec2( ( int( index ) & 31 ) + .5,
					( int( index ) / 32 ) + .5 );
*/
	vec3 blendshape = texture2DRect( blendshapes, uv ).xyz;
	//gl_Position = gl_ModelViewProjectionMatrix * ( gl_Vertex + blend * vec4( blendshape, 0 ) );
	gl_Position = gl_ModelViewProjectionMatrix * mix( gl_Vertex, vec4( blendshape, 1 ), blend );
	gl_TexCoord[ 0 ] = gl_MultiTexCoord0;
	gl_FrontColor = gl_Color;
}

