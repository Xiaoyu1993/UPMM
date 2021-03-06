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

#if !defined(__PSSMLT_H)
#define __PSSMLT_H

#include <mitsuba/bidir/pathsampler.h>
#include <mitsuba/core/bitmap.h>

MTS_NAMESPACE_BEGIN

/* ==================================================================== */
/*                         Configuration storage                        */
/* ==================================================================== */

/**
 * \brief Stores all configuration parameters used by
 * the MLT rendering implementation
 */
struct VCMConfiguration {
	int maxDepth, blockSize, borderSize;
	bool lightImage;
	bool sampleDirect;
	bool showWeighted;
	size_t sampleCount;
	Vector2i cropSize;
	int rrDepth;

	size_t timeout;
	int workUnits;
	Float initialRadius;
	Float radiusScale;

	bool useVM;
	bool useVC;

	bool enableSeparateDump;
	bool enableProgressiveDump;

	inline VCMConfiguration() { }

	inline VCMConfiguration(Stream *stream) {
		maxDepth = stream->readInt();
		blockSize = stream->readInt();
		lightImage = stream->readBool();
		sampleDirect = stream->readBool();
		showWeighted = stream->readBool();
		sampleCount = stream->readSize();
		cropSize = Vector2i(stream);
		rrDepth = stream->readInt();
		initialRadius = stream->readFloat();
		radiusScale = stream->readFloat();
		workUnits = stream->readInt();
		timeout = stream->readSize();
		useVC = stream->readBool();
		useVM = stream->readBool();
		enableSeparateDump = stream->readBool();
		enableProgressiveDump = stream->readBool();
	}

	inline void serialize(Stream *stream) const {
		stream->writeInt(maxDepth);
		stream->writeInt(blockSize);
		stream->writeBool(lightImage);
		stream->writeBool(sampleDirect);
		stream->writeBool(showWeighted);
		stream->writeSize(sampleCount);
		cropSize.serialize(stream);
		stream->writeInt(rrDepth);
		stream->writeFloat(initialRadius);
		stream->writeFloat(radiusScale);
		stream->writeInt(workUnits);
		stream->writeSize(timeout);
		stream->writeBool(useVC);
		stream->writeBool(useVM);
		stream->writeBool(enableSeparateDump);
		stream->writeBool(enableProgressiveDump);
	}

	void dump() const {
		SLog(EDebug, "Bidirectional path tracer configuration:");
		SLog(EDebug, "   Maximum path depth          : %i", maxDepth);
		SLog(EDebug, "   Image size                  : %ix%i",
			cropSize.x, cropSize.y);
		SLog(EDebug, "   Direct sampling strategies  : %s",
			sampleDirect ? "yes" : "no");
		SLog(EDebug, "   Generate light image        : %s",
			lightImage ? "yes" : "no");
		SLog(EDebug, "   Russian roulette depth      : %i", rrDepth);
		SLog(EDebug, "   Block size                  : %i", blockSize);
		SLog(EDebug, "   Number of samples           : " SIZE_T_FMT, sampleCount);
		SLog(EDebug, "   Initial radius of vertex merging:   %f", initialRadius);
		SLog(EDebug, "   Scale of initial radius for gathering:	%f", radiusScale);
		SLog(EDebug, "   Total number of work units  : %i", workUnits);
		SLog(EDebug, "   Timeout                     : " SIZE_T_FMT, timeout);
		SLog(EDebug, "   Enable separate dump   : %s", enableSeparateDump ? "yes" : "no");
		SLog(EDebug, "   Enable progressive dump   : %s", enableProgressiveDump ? "yes" : "no");
	}
};

MTS_NAMESPACE_END

#endif /* __PSSMLT_H */
