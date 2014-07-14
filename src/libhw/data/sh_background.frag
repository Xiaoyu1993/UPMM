/*
    This file is part of Mitsuba, a physically based rendering system.

    Copyright (c) 2007-2012 by Wenzel Jakob and others.

    Mitsuba is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License Version 3
    as published by the Free Software Foundation.

    Mitsuba is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#extension GL_EXT_gpu_shader4 : enable

varying vec3 position;

#if defined(DIRECTIONAL_CAMERA)
	uniform vec3 camDirection;
#else
	uniform vec3 camPosition;
#endif

uniform float emitterScale;

{{ SUPPLEMENTAL CODE }}

void main() {
	vec3 result;

	#if !defined(DIRECTIONAL_CAMERA)
		result = BACKGROUND_EVAL_NAME(normalize(position - camPosition));
	#else
		result = BACKGROUND_EVAL_NAME(camDirection);
	#endif

	gl_FragColor = vec4(result * emitterScale, 1.0);
}
