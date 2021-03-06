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

#pragma once
#if !defined(__MITSUBA_BIDIR_PATHSAMPLER_H_)
#define __MITSUBA_BIDIR_PATHSAMPLER_H_

#include <mitsuba/bidir/path.h>
#include <boost/function.hpp>
#include <mitsuba/core/fstream.h>
#include <mitsuba/core/kdtree.h>

MTS_NAMESPACE_BEGIN

//#define UPM_DEBUG 1
// #define UPM_DEBUG_HARD

/*
*	Misc for VCM
*/
enum EMisTech {
	EDIR = 0,
	EVC = 1,
	EPM = 2,
	EMisTechs = 3
};
struct MTS_EXPORT_BIDIR MisState{
	float state[EMisTechs];
	MisState(){
		state[EDIR] = 1.f;
		state[EVC] = 0.f;
		state[EPM] = 0.f;
	}
	float& operator[](EMisTech tech){
		return state[tech];
	}
};
struct LightVertex{
	Spectrum importanceWeight;
	Vector wo;
	MisState emitterState;
	inline  LightVertex(const PathVertex *vs, const PathVertex *vsPred,
		const MisState &state, Spectrum _wgt, ETransportMode mode = EImportance, Point2 samplePos = Point2(0.f)) :
		emitterState(state), importanceWeight(_wgt){
		if (vs->isSurfaceInteraction())
			wo = normalize(vsPred->getPosition() - vs->getPosition());
		
		// for importon
		if (mode == ERadiance){
			wo.x = samplePos.x;
			wo.y = samplePos.y;
		}		
	}
};
struct LightVertexExt{
	int depth;
	const Shape *shape;
	Point3 position;
	Vector shFrameN;
	Vector shFrameS;
	Vector geoFrameN;
	uint8_t measure;
	uint8_t type;
	Float pdfImp;
	Float pdfRad;
	bool degenerate;
	
	inline LightVertexExt(const PathVertex *vs, const PathVertex *vsPred, int _depth){
		if (vs->isEmitterSample() || vs->isSensorSample()){
			const PositionSamplingRecord &pRec = vs->getPositionSamplingRecord();
			shape = (const Shape*)pRec.object;
			geoFrameN = pRec.n;
			position = pRec.p;
		}
		else if (vs->isSurfaceInteraction()){
			const Intersection &its = vs->getIntersection();			
			shape = its.shape;
			position = its.p;
			shFrameS = its.shFrame.s;
			shFrameN = its.shFrame.n;
			geoFrameN = its.geoFrame.n;			
		}
		depth = _depth;
		measure = vs->measure;
		degenerate = vs->degenerate;
		type = vs->type;
		pdfImp = vs->pdf[EImportance];
		pdfRad = vs->pdf[ERadiance];
	}
	void expand(PathVertex* vs){
		memset(vs, 0, sizeof(PathVertex));
		vs->type = type;
		vs->measure = measure;
		vs->degenerate = degenerate;
		vs->pdf[EImportance] = pdfImp;
		vs->pdf[ERadiance] = pdfRad;
		if (vs->isEmitterSample() || vs->isSensorSample()){
			PositionSamplingRecord &pRec = vs->getPositionSamplingRecord();
			pRec.object = (const ConfigurableObject *)shape;
			pRec.p = position;
			pRec.n = geoFrameN;
		}
		else if (vs->isSurfaceInteraction()){
			Intersection &itp = vs->getIntersection();
			itp.p = position;
			itp.shFrame.s = shFrameS;
			itp.shFrame.n = shFrameN;
			itp.shFrame.t = cross(itp.shFrame.n, itp.shFrame.s);
			itp.geoFrame = Frame(geoFrameN);
			itp.setShapePointer(shape);
		}
	}
};
struct LightPathNodeData{
	int depth;
	size_t vertexIndex;
};
struct LightPathNode : public SimpleKDNode < Point, LightPathNodeData > {

	inline LightPathNode(){}

	inline  LightPathNode(const Point3 p, size_t vertexIndex, int depth){
		position = p;
		data.vertexIndex = vertexIndex;
		data.depth = depth;
	}
	inline LightPathNode(Stream *stream) {
		// TODO
	}
	void serialize(Stream *stream) const {
		// TODO
	}
};
typedef PointKDTree<LightPathNode>		LightPathTree;
typedef LightPathTree::IndexType     IndexType;
typedef LightPathTree::SearchResult SearchResult;


/*
*	Misc for CMLT
*/
enum EConnectionFlags {
	EConnectVisibility = 1,
	EConnectGeometry = 2,
	EConnectBRDF = 4,
	EConnectMis = 8,
	EConnectImportance = 16,
	EConnectRadiance = 32,
	EConnectAll = 64
};
struct MTS_EXPORT_BIDIR SplatListImp {
	/// Represents a screen-space splat produced by a path sampling technique
	typedef std::pair<Point2, Spectrum> Splat;

	/// A series of splats associated with the current sample
	std::vector<Splat> splats;
	std::vector<Spectrum> importances;
	/// Combined importance of all splats in this sample
	Float importance;
	/// Total number of samples in the splat list
	int nSamples;

	inline SplatListImp() : importance(1.0f), nSamples(0) { }

	/// for arbitrary Metropolis importance functions	
	/// Appends a splat entry to the list
	inline void append(const Point2 &samplePos, const Spectrum &value, const Spectrum &imp) {
		splats.push_back(std::make_pair(samplePos, value));
		importances.push_back(imp);
		importance += imp.getLuminance();
		++nSamples;
	}

	/// Increases the contribution of an existing splat
	inline void accum(size_t i, const Spectrum &value, const Spectrum &imp) {
		splats[i].second += value;
		importances[i] += imp;
		importance += imp.getLuminance();
		++nSamples;
	}

	/// Returns the number of contributions
	inline size_t size() const {
		return splats.size();
	}

	/// Clear the splat list
	inline void clear() {
		importance = 1;
		nSamples = 0;
		splats.clear();
		importances.clear();
	}

	/// Return the position associated with a splat in the list
	inline const Point2 &getPosition(size_t i) const { return splats[i].first; }

	/// Return the spectral contribution associated with a splat in the list
	inline const Spectrum &getValue(size_t i) const { return splats[i].second; }

	inline const Spectrum &getImportance(size_t i) const { return importances[i]; }

	/**
	* \brief Normalize the splat list
	*
	* This function divides all splats so that they have unit
	* luminance (though it leaves the \c luminance field untouched).
	* When given an optional importance map in 2-stage MLT approaches,
	* it divides the splat values by the associated importance
	* map values
	*/
	void normalize(const Bitmap *importanceMap = NULL){
		if (importanceMap) {
			BDAssert(false); // not implemented yet
		}
		if (importance > 0) {
			/* Normalize the contributions */
			Float invImportance = 1.0f / importance;
			for (size_t i = 0; i < splats.size(); ++i){
				splats[i].second *= invImportance;
				importances[i] *= invImportance;
			}
		}
	}

	/// Return a string representation
	std::string toString() const;	
};

/* ==================================================================== */
/*                         Work result for UPM                        */
/* ==================================================================== */
class UPMWorkResult : public WorkResult {
public:
	UPMWorkResult(const int width, const int height, const int maxDepth, const ReconstructionFilter *rfilter, bool guided = false): m_guided(guided){
		/* Stores the 'camera image' -- this can be blocked when
		spreading out work to multiple workers */
		Vector2i blockSize = Vector2i(width, height);

		m_block = new ImageBlock(Bitmap::ESpectrum, blockSize, rfilter);
		m_block->setOffset(Point2i(0, 0));
		m_block->setSize(blockSize);

		/* When debug mode is active, we additionally create
		full-resolution bitmaps storing the contributions of
		each individual sampling strategy */
#if UPM_DEBUG == 1
		m_debugBlocks.resize(
			maxDepth*(5 + maxDepth) / 2);
		m_debugBlocksM.resize(
			maxDepth*(5 + maxDepth) / 2);		

		for (size_t i = 0; i<m_debugBlocks.size(); ++i) {
			m_debugBlocks[i] = new ImageBlock(
				Bitmap::ESpectrum, blockSize, rfilter);
			m_debugBlocks[i]->setOffset(Point2i(0, 0));
			m_debugBlocks[i]->setSize(blockSize);
		}
		for (size_t i = 0; i < m_debugBlocksM.size(); ++i) {
			m_debugBlocksM[i] = new ImageBlock(
				Bitmap::ESpectrum, blockSize, rfilter);
			m_debugBlocksM[i]->setOffset(Point2i(0, 0));
			m_debugBlocksM[i]->setSize(blockSize);
		}

		m_block_vc = new ImageBlock(Bitmap::ESpectrum, blockSize, rfilter);
		m_block_vm = new ImageBlock(Bitmap::ESpectrum, blockSize, rfilter);

		tentativeDistribution.resize(100);
		for (int i = 0; i < 100; i++)
			tentativeDistribution[i] = 0.f;
#endif
		sampleCount = 0;

		m_timeTraceKernel = new Timer(false);
		m_timeBoundProb = new Timer(false);
		m_timeBoundSurfaceProb = new Timer(false);
		m_timeBoundSample = new Timer(false);
		m_timeProbDistrib = new Timer(false);
		m_timeProbGMM = new Timer(false);
		m_timeProbLobe = new Timer(false);
	}

	// Clear the contents of the work result
	void clear(){
#if UPM_DEBUG == 1
		for (size_t i = 0; i < m_debugBlocks.size(); ++i)
			m_debugBlocks[i]->clear();
		for (size_t i = 0; i < m_debugBlocksM.size(); ++i)
			m_debugBlocksM[i]->clear();

		m_block_vc->clear();
		m_block_vm->clear();

		tentativeDistribution.resize(100);
		for (int i = 0; i < 100; i++)
			tentativeDistribution[i] = 0.f;
#endif
		m_block->clear();
		sampleCount = 0;
	}

	/// Fill the work result with content acquired from a binary data stream
	virtual void load(Stream *stream){
#if UPM_DEBUG == 1
		for (size_t i = 0; i < m_debugBlocks.size(); ++i)
			m_debugBlocks[i]->load(stream);
		for (size_t i = 0; i < m_debugBlocksM.size(); ++i)
			m_debugBlocksM[i]->load(stream);

		m_block_vc->load(stream);
		m_block_vm->load(stream);
#endif
		m_block->load(stream);
	}

	/// Serialize a work result to a binary data stream
	virtual void save(Stream *stream) const{
#if UPM_DEBUG == 1
		for (size_t i = 0; i < m_debugBlocks.size(); ++i)
			m_debugBlocks[i]->save(stream);
		for (size_t i = 0; i < m_debugBlocksM.size(); ++i)
			m_debugBlocksM[i]->save(stream);

		m_block_vc->save(stream);
		m_block_vm->save(stream);
#endif
		m_block->save(stream);
	}

	/// Aaccumulate another work result into this one
	void put(const UPMWorkResult *workResult){
#if UPM_DEBUG == 1
		for (size_t i = 0; i < m_debugBlocks.size(); ++i)
			m_debugBlocks[i]->put(workResult->m_debugBlocks[i].get());
		for (size_t i = 0; i < m_debugBlocksM.size(); ++i)
			m_debugBlocksM[i]->put(workResult->m_debugBlocksM[i].get());

		m_block_vc->put(workResult->m_block_vc.get());
		m_block_vm->put(workResult->m_block_vm.get());

		for (int i = 0; i < 100; i++)
			tentativeDistribution[i] += workResult->tentativeDistribution[i];
#endif
		m_block->put(workResult->m_block.get());
		sampleCount += workResult->getSampleCount();
	}

#if UPM_DEBUG == 1
	/* In debug mode, this function allows to dump the contributions of
	the individual sampling strategies to a series of images */
	void dump(const int width, const int height, const int maxDepth,
		const fs::path &prefix, const fs::path &stem,
		bool useVC, bool useVM) const {
		Float weight = 1.f / (Float)sampleCount;
		char* algorithm;
		if (useVM && useVC) algorithm = (m_guided) ? "gupm" : "upmc";
		else if (useVC) algorithm = "vc";
		else if (useVM) algorithm = "upm";
		else
			algorithm = "none";
		Vector2i blockSize = Vector2i(width, height);
		Bitmap* kmap = new Bitmap(Bitmap::ESpectrum, Bitmap::EFloat, blockSize + Vector2i(0), -1);		
		for (int k = 1; k <= maxDepth; ++k) {
			kmap->clear();
			for (int t = 0; t <= k + 1; ++t) {
				size_t s = k + 1 - t;
				Bitmap *bitmap = const_cast<Bitmap *>(m_debugBlocks[strategyIndex(s, t)]->getBitmap());
				if (bitmap->average().isZero()) continue;
				kmap->accumulate(bitmap);
				ref<Bitmap> ldrBitmap = bitmap->convert(Bitmap::ERGB, Bitmap::EFloat32, -1, weight);
				fs::path filename =
					prefix / fs::path(formatString("%s_%s_k%02i_s%02i_t%02i.pfm", stem.filename().string().c_str(), algorithm, k, s, t));
				ref<FileStream> targetFile = new FileStream(filename,
					FileStream::ETruncReadWrite);
				ldrBitmap->write(Bitmap::EPFM, targetFile, 1);
			}
			ref<Bitmap> ldrBitmap = kmap->convert(Bitmap::ERGB, Bitmap::EFloat32, -1, weight);
			fs::path filename =
				prefix / fs::path(formatString("%s_%s_k%02i.pfm", stem.filename().string().c_str(), algorithm, k));
			ref<FileStream> targetFile = new FileStream(filename, FileStream::ETruncReadWrite);
			ldrBitmap->write(Bitmap::EPFM, targetFile, 1);

			for (int t = 0; t <= k + 1; ++t) {
				size_t s = k + 1 - t;
				Bitmap *bitmap = const_cast<Bitmap *>(m_debugBlocksM[strategyIndex(s, t)]->getBitmap());
				if (bitmap->average().isZero()) continue;
				ref<Bitmap> ldrBitmap = bitmap->convert(Bitmap::ERGB, Bitmap::EFloat32, -1, weight);
				fs::path filename =
					prefix / fs::path(formatString("%s_%s_nm_k%02i_s%02i_t%02i.pfm", stem.filename().string().c_str(), algorithm, k, s, t));
				ref<FileStream> targetFile = new FileStream(filename,
					FileStream::ETruncReadWrite);
				ldrBitmap->write(Bitmap::EPFM, targetFile, 1);
			}
		}
		Bitmap *bitmap = const_cast<Bitmap *>(m_block_vc->getBitmap());
		if (!bitmap->average().isZero()){
			ref<Bitmap> ldrBitmap = bitmap->convert(Bitmap::ERGB, Bitmap::EFloat32, 1.0, weight);
			fs::path filename =
				prefix / fs::path(formatString("%s_%s_vc.pfm", stem.filename().string().c_str(), algorithm));
			ref<FileStream> targetFile = new FileStream(filename,
				FileStream::ETruncReadWrite);
			ldrBitmap->write(Bitmap::EPFM, targetFile, 1);
		}
		bitmap = const_cast<Bitmap *>(m_block_vm->getBitmap());
		if (!bitmap->average().isZero()){
			ref<Bitmap> ldrBitmap = bitmap->convert(Bitmap::ERGB, Bitmap::EFloat32, 1.0, weight);
			fs::path filename =
				prefix / fs::path(formatString("%s_%s_vm.pfm", stem.filename().string().c_str(), algorithm));
			ref<FileStream> targetFile = new FileStream(filename,
				FileStream::ETruncReadWrite);
			ldrBitmap->write(Bitmap::EPFM, targetFile, 1);
		}

		if (tentativeDistribution.size() > 0){
			fs::path filename = prefix / fs::path(formatString("%s_distribution.csv", stem.filename().string().c_str()));
			ref<FileStream> stream = new FileStream(filename, FileStream::ETruncReadWrite);
			for (int i = 0; i < 100; i++){
				std::ostringstream oss;
				oss << i << ','  << tentativeDistribution[i] << '\n';				
				std::string itemi = oss.str();
				stream->write(itemi.c_str(), itemi.length());
			}
		}
	}

	inline void putDebugSample(int s, int t, const Point2 &sample, const Spectrum &spec) {
		m_debugBlocks[strategyIndex(s, t)]->put(sample, (const Float *)&spec);
	}
	inline void putDebugSampleM(int s, int t, const Point2 &sample, const Spectrum &spec) {
		return;
		m_debugBlocksM[strategyIndex(s, t)]->put(sample, (const Float *)&spec);
	}
	inline void putDebugSampleVM(const Point2 &sample, const Spectrum &spec) {
		m_block_vm->put(sample, (const Float *)&spec);
	}
	inline void putDebugSampleVC(const Point2 &sample, const Spectrum &spec) {
		m_block_vc->put(sample, (const Float *)&spec);
	}
	inline void putTentativeSample(const size_t t){
		int index = (int)std::min((size_t)tentativeDistribution.size() - 1, t);
		tentativeDistribution[index] += 1.f;
	}

	inline void progressiveDump(const int width, const int height, const int maxDepth,
		const fs::path &prefix, const fs::path &stem, const int index,
		const size_t  actualSampleCount, const bool isUPM) const{
		Float weight = 1.f / (Float)actualSampleCount;
		Bitmap *bitmap = const_cast<Bitmap *>(m_block_vc->getBitmap());
		if (!bitmap->average().isZero()){
			ref<Bitmap> ldrBitmap = bitmap->convert(Bitmap::ERGB, Bitmap::EFloat32, 1.0, weight);
			fs::path filename =
				prefix / fs::path(formatString("%s_%s_vc_progress%d.pfm", stem.filename().string().c_str(), isUPM ? "upm" : "vcm", index));
			ref<FileStream> targetFile = new FileStream(filename,
				FileStream::ETruncReadWrite);
			ldrBitmap->write(Bitmap::EPFM, targetFile, 1);
		}
		bitmap = const_cast<Bitmap *>(m_block_vm->getBitmap());
		if (!bitmap->average().isZero()){
			ref<Bitmap> ldrBitmap = bitmap->convert(Bitmap::ERGB, Bitmap::EFloat32, 1.0, weight);
			fs::path filename =
				prefix / fs::path(formatString("%s_%s_vm_progress%d.pfm", stem.filename().string().c_str(), isUPM ? "upm" : "vcm", index));
			ref<FileStream> targetFile = new FileStream(filename,
				FileStream::ETruncReadWrite);
			ldrBitmap->write(Bitmap::EPFM, targetFile, 1);
		}
		bitmap = const_cast<Bitmap *>(m_block->getBitmap());
		if (!bitmap->average().isZero()){
			ref<Bitmap> ldrBitmap = bitmap->convert(Bitmap::ERGB, Bitmap::EFloat32, 1.0, weight);
			fs::path filename =
				prefix / fs::path(formatString("%s_%s_vcm_progress%d.pfm", stem.filename().string().c_str(), isUPM ? "upm" : "vcm", index));
			ref<FileStream> targetFile = new FileStream(filename,
				FileStream::ETruncReadWrite);
			ldrBitmap->write(Bitmap::EPFM, targetFile, 1);
		}
	}
#endif

	inline void putSample(const Point2 &sample, const Float *value) {
		m_block->put(sample, value);
	}

	// 	inline void putLightSample(const Point2 &sample, const Spectrum &spec) {
	// 		m_lightImage->put(sample, spec, 1.0f);
	// 	}

	inline const ImageBlock *getImageBlock() const {
		return m_block.get();
	}
	inline ImageBlock *getImageBlock() {
		return m_block.get();
	}

	// 	inline const ImageBlock *getLightImage() const {
	// 		return m_lightImage.get();
	// 	}

	inline void setSize(const Vector2i &size) {
		m_block->setSize(size);
	}

	inline void setOffset(const Point2i &offset) {
		m_block->setOffset(offset);
	}

	void accumSampleCount(size_t count){
		sampleCount += count;
	}
	size_t getSampleCount() const{
		return sampleCount;
	}

	/// Return a string representation
	std::string toString() const {
		return m_block->toString();
	}

	MTS_DECLARE_CLASS()
protected:
	/// Virtual destructor
	virtual ~UPMWorkResult(){}

	inline int strategyIndex(int s, int t) const {
		int above = s + t - 2;
		return s + above*(5 + above) / 2;
	}
protected:
#if UPM_DEBUG == 1	
	ref_vector<ImageBlock> m_debugBlocks;
	ref_vector<ImageBlock> m_debugBlocksM;
	ref<ImageBlock> m_block_vc;
	ref<ImageBlock> m_block_vm;
	std::vector<Float> tentativeDistribution;
#endif
	size_t sampleCount;
	ref<ImageBlock> m_block; // , m_lightImage;
	bool m_guided;

public:
	ref<Timer> m_timeTraceKernel;
	ref<Timer> m_timeBoundProb;
	ref<Timer> m_timeBoundSurfaceProb;
	ref<Timer> m_timeBoundSample;
	ref<Timer> m_timeProbDistrib;
	ref<Timer> m_timeProbGMM;
	ref<Timer> m_timeProbLobe;
};

/**
 * \brief Implements a sampling strategy that is able to produce paths using
 * bidirectional path tracing or unidirectional volumetric path tracing.
 *
 * This versatile class does the heavy lifting under the hood of Mitsuba's
 * PSSMLT implementation. It is also used to provide the Veach-MLT
 * implementation with a luminance estimate and seed paths.
 *
 * \author Wenzel Jakob
 * \ingroup libbidir
 */
class MTS_EXPORT_BIDIR PathSampler : public Object {
public:
	/**
	 * \brief Callback type for use with \ref samplePaths()
	 *
	 * The arguments are (s, t, weight, path) where \c s denotes the number
	 * of steps from the emitter, \c is the number of steps from the sensor,
	 * and \c weight contains the importance weight associated with the sample.
	 */
	typedef boost::function<void (int, int, Float, Path &)> PathCallback;

	/// Specifies the sampling algorithm that is internally used
	enum ETechnique {
		/// Bidirectional path tracing
		EBidirectional,

		/// Unidirectional path tracing (via the 'volpath' plugin)
		EUnidirectional
	};

	/**
	 * Construct a new path sampler
	 *
	 * \param technique
	 *     What path generation technique should be used (unidirectional
	 *     or bidirectional path tracing?)
	 *
	 * \param scene
	 *     \ref A pointer to the underlying scene
	 *
	 * \param emitterSampler
	 *     Sample generator that should be used for the random walk
	 *     from the emitter direction
	 *
	 * \param sensorSampler
	 *     Sample generator that should be used for the random walk
	 *     from the sensor direction
	 *
	 * \param directSampler
	 *     Sample generator that should be used for direct sampling
	 *     strategies (or \c NULL, when \c sampleDirect=\c false)
	 *
	 * \param maxDepth
	 *     Maximum path depth to be visualized (-1==infinite)
	 *
	 * \param rrDepth
	 *     Depth to begin using russian roulette
	 *
	 * \param excludeDirectIllum
	 *     If set to true, the direct illumination component will
	 *     be ignored. Note that this parameter is unrelated
	 *     to the next one (\a sampleDirect) although they are
	 *     named similarly.
	 *
	 * \param sampleDirect
	 *     When this parameter is set to true, specialized direct
	 *     sampling strategies are used for s=1 and t=1 paths.
	 *
	 * \param lightImage
	 *    Denotes whether or not rendering strategies that require a 'light image'
	 *    (specifically, those with <tt>t==0</tt> or <tt>t==1</tt>) are included
	 *    in the rendering process.
	 */
	PathSampler(ETechnique technique, const Scene *scene, Sampler *emitterSampler,
		Sampler *sensorSampler, Sampler *directSampler, int maxDepth, int rrDepth,
		bool excludeDirectIllum, bool sampleDirect, bool lightImage = true,
		Sampler *lightPathSampler = NULL);

	/**
	 * \brief Generate a sample using the configured sampling strategy
	 *
	 * The result is stored as a series of screen-space splats (pixel position
	 * and spectral value pairs) within the parameter \c list. These can be
	 * used to implement algorithms like Bidirectional Path Tracing or Primary
	 * Sample Space MLT.
	 *
	 * \param offset
	 *    Specifies the desired integer pixel position of the sample. The special
	 *    value <tt>Point2i(-1)</tt> results in uniform sampling in screen space.
	 *
	 * \param list
	 *    Output parameter that will receive a list of splats
	 */
	void sampleSplats(const Point2i &offset, SplatList &list);

	/**
	 * \brief Sample a series of paths and invoke the specified callback
	 * function for each one.
	 *
	 * This function is similar to \ref sampleSplats(), but instead of
	 * returning only the contribution of the samples paths in the form of
	 * screen-space "splats", it returns the actual paths by invoking a
	 * specified callback function multiple times.
	 *
	 * This function is currently only implemented for the bidirectional
	 * sampling strategy -- i.e. it cannot be used with the unidirectional
	 * path tracer.
	 *
	 * \param offset
	 *    Specifies the desired integer pixel position of the sample. The special
	 *    value <tt>Point2i(-1)</tt> results in uniform sampling in screen space.
	 *
	 * \param pathCallback
	 *    A callback function that will be invoked once for each
	 *    path generated by the BDPT sampling strategy. The first argument
	 *    specifies the path importance weight.
	 */
	void samplePaths(const Point2i &offset, PathCallback &callback);

	/**
	 * \brief Generates a sequence of seeds that are suitable for
	 * starting a MLT Markov Chain
	 *
	 * This function additionally computes the average luminance
	 * over the image plane.
	 *
	 * \param sampleCount
	 *     The number of luminance samples that will be taken
	 * \param seedCount
	 *     The desired number of MLT seeds (must be > \c sampleCount)
	 * \param fineGrained
	 *     This parameter only matters when the technique is set to
	 *     \ref EBidirectional. It specifies whether to generate \ref PathSeed
	 *     records at the granularity of entire sensor/emitter subpaths or at
	 *     the granularity of their constituent sampling strategies.
	 * \param seeds
	 *     A vector of resulting MLT seeds
	 * \return The average luminance over the image plane
	 */
	Float generateSeeds(size_t sampleCount, size_t seedCount,
			bool fineGrained, const Bitmap *importanceMap,
			std::vector<PathSeed> &seeds);

	/**
	 * \brief Compute the average luminance over the image plane
	 * \param sampleCount
	 *     The number of luminance samples that will be taken
	 */
	Float computeAverageLuminance(size_t sampleCount);

	/**
	 * \brief Reconstruct a path from a \ref PathSeed record
	 *
	 * Given a \ref PathSeed data structure, this function rewinds
	 * the random number stream of the underlying \ref ReplayableSampler
	 * to the indicated position and recreates the associated path.
	 */
	void reconstructPath(const PathSeed &seed, 
		const Bitmap *importanceMap, Path &result);

	/// Return the underlying memory pool
	inline MemoryPool &getMemoryPool() { return m_pool; }

	/// for Connection MLT
	void sampleSplatsConnection(const Point2i &offset, SplatListImp &list, const int connectionFlag);
	Float generateSeedsConnection(size_t sampleCount, size_t seedCount,
		bool fineGrained, const Bitmap *importanceMap,
		std::vector<PathSeed> &seeds, const int connectionFlag);
	int getConnectionFlag(bool connectionImportance, bool connectionRadiance, bool connectionVisibility,
		bool connectionMIS, bool connectionBSDFs, bool connectionGeometry, bool connectionFull);

	/// for VCM
	void gatherLightPaths(const bool useVC, const bool useVM, const float gatherRadius, const int nsample, ImageBlock* lightImage = NULL);
	void sampleSplatsVCM(const bool useVC, const bool useVM, const float gatherRadius, const Point2i &offset, const size_t cameraPathIndex, SplatList &list);

	/// for UPM
	void gatherLightPathsUPM(const bool useVC, const bool useVM, const float gatherRadius, const int nsample, UPMWorkResult *wr, ImageBlock *batres = NULL, Float rejectionProb = 0.f);
	void sampleSplatsUPM(UPMWorkResult *wr, const float gatherRadius, const Point2i &offset, const size_t cameraPathIndex, SplatList &list, 
		bool useVC = false, bool useVM = true, 
		Float rejectionProb = 0.f, size_t clampThreshold = 100, bool useVCMPdf = false);

	/// for Extended PSSMLT
	void gatherCameraPathsUPM(const bool useVC, const bool useVM, const float gatherRadius);
	Float generateSeedsExtend(const bool useVC, const bool useVM, const float gatherRadius, 
		size_t sampleCount, size_t seedCount, std::vector<PathSeed> &seeds);
	void sampleSplatsExtend(const bool useVC, const bool useVM, const float gatherRadius, 
		const Point2i &offset, SplatList &list);
	void setIndependentSampler(Sampler* sampler){
		m_lightPathSampler = sampler;
	}

	/// for MMLT
	Float generateSeedsSpec(size_t sampleCount, size_t seedCount,
		bool fineGrained, const Bitmap *importanceMap,
		std::vector<PathSeed> &seeds, const int pathLength);
	void PathSampler::sampleSplatsSpec(const Point2i &offset, SplatList &list, 
		const int SpecifiedNumLightVertices, const int SpecifiedNumEyeVertices);

	MTS_DECLARE_CLASS()
protected:
	/// Virtual destructor
	virtual ~PathSampler();
//protected:
public: // temprorily open them for guided upm
	ETechnique m_technique;
	ref<const Scene> m_scene;
	ref<SamplingIntegrator> m_integrator;
	ref<Sampler> m_emitterSampler;
	ref<Sampler> m_sensorSampler;
	ref<Sampler> m_directSampler;
	int m_maxDepth;
	int m_rrDepth;
	bool m_excludeDirectIllum;
	bool m_sampleDirect;
	bool m_lightImage;
	int m_emitterDepth, m_sensorDepth;
	Path m_emitterSubpath, m_sensorSubpath;
	Path m_connectionSubpath, m_fullPath;
	MemoryPool m_pool;

	ref<Sampler> m_lightPathSampler; // independent sampler for photon and importon trace

	// VCM
	size_t m_lightPathNum;	
	LightPathTree m_lightPathTree;
	std::vector<LightVertex> m_lightVertices;
	std::vector<LightVertexExt> m_lightVerticesExt;
	std::vector<size_t> m_lightPathEnds;

	// EPSSMLT
	LightPathTree m_cameraPathTree;
	std::vector<LightVertex> m_cameraVertices;
	std::vector<LightVertexExt> m_cameraVerticesExt;
};

/**
 * \brief Stores information required to re-create a seed path (e.g. for MLT)
 *
 * This class makes it possible to transmit a path over the network or store
 * it locally, while requiring very little storage to do so. This is done by
 * describing a path using an index into a random number stream, which allows
 * to generate it cheaply when needed.
 */
struct PathSeed {
	size_t sampleIndex; ///< Index into a rewindable random number stream
	Float luminance;    ///< Luminance value of the path (for sanity checks)
	int s;              ///< Number of steps from the luminaire
	int t;              ///< Number of steps from the eye

	inline PathSeed() { }

	inline PathSeed(size_t sampleIndex, Float luminance, int s = 0, int t = 0)
		: sampleIndex(sampleIndex), luminance(luminance), s(s), t(t) { }

	inline PathSeed(Stream *stream) {
		sampleIndex = stream->readSize();
		luminance = stream->readFloat();
		s = stream->readInt();
		t = stream->readInt();
	}

	void serialize(Stream *stream) const {
		stream->writeSize(sampleIndex);
		stream->writeFloat(luminance);
		stream->writeInt(s);
		stream->writeInt(t);
	}

	std::string toString() const {
		std::ostringstream oss;
		oss << "PathSeed[" << endl
			<< "  sampleIndex = " << sampleIndex << "," << endl
			<< "  luminance = " << luminance << "," << endl
			<< "  s = " << s << "," << endl
			<< "  t = " << t << endl
			<< "]";
		return oss.str();
	}
};

/**
 * MLT work unit -- wraps a \ref PathSeed into a
 * \ref WorkUnit instance.
 */
class SeedWorkUnit : public WorkUnit {
public:
	inline void set(const WorkUnit *wu) {
		m_id = static_cast<const SeedWorkUnit *>(wu)->m_id;
		m_seed = static_cast<const SeedWorkUnit *>(wu)->m_seed;
		m_timeout = static_cast<const SeedWorkUnit *>(wu)->m_timeout;
		m_worknum = static_cast<const SeedWorkUnit *>(wu)->m_worknum;
	}	

	inline const PathSeed &getSeed() const {
		return m_seed;
	}

	inline void setSeed(const PathSeed &seed) {
		m_seed = seed;
	}

	inline size_t getTimeout() const {
		return m_timeout;
	}

	inline void setTimeout(size_t timeout) {
		m_timeout = timeout;
	}

	inline void load(Stream *stream) {
		m_seed = PathSeed(stream);
		m_timeout = stream->readSize();
	}

	inline void save(Stream *stream) const {
		m_seed.serialize(stream);
		stream->writeSize(m_timeout);
	}

	inline std::string toString() const {
		return "SeedWorkUnit[]";
	}

	inline const int getID() const{
		return m_id;
	}
	inline const void setID(int id){
		m_id = id;
	}

	inline const int getTotalWorkNum() const{
		return m_worknum;
	}
	inline void setTotalWorkNum(int num){
		m_worknum = num;
	}

	MTS_DECLARE_CLASS()
private:
	int m_id;
	int m_worknum;
	PathSeed m_seed;
	size_t m_timeout;
};

/**
 * \brief List storage for the image-space contributions ("splats") of a
 * path sample generated using \ref PathSampler.
 */
struct MTS_EXPORT_BIDIR SplatList {
	/// Represents a screen-space splat produced by a path sampling technique
	typedef std::pair<Point2, Spectrum> Splat;

	/// A series of splats associated with the current sample
	std::vector<Splat> splats;
	/// Combined luminance of all splats in this sample
	Float luminance;
	/// Total number of samples in the splat list
	int nSamples;

	inline SplatList() : luminance(0.0f), nSamples(0) { }

	/// Appends a splat entry to the list
	inline void append(const Point2 &samplePos, const Spectrum &value) {
		splats.push_back(std::make_pair(samplePos, value));
		luminance += value.getLuminance();
		++nSamples;
	}

	/// Increases the contribution of an existing splat
	inline void accum(size_t i, const Spectrum &value) {
		splats[i].second += value;
		luminance += value.getLuminance();
		++nSamples;
	}

	/// Returns the number of contributions
	inline size_t size() const {
		return splats.size();
	}

	/// Clear the splat list
	inline void clear() {
		luminance = 0;
		nSamples = 0;
		splats.clear();
	}

	/// Return the position associated with a splat in the list
	inline const Point2 &getPosition(size_t i) const { return splats[i].first; }

	/// Return the spectral contribution associated with a splat in the list
	inline const Spectrum &getValue(size_t i) const { return splats[i].second; }

	/**
	 * \brief Normalize the splat list
	 *
	 * This function divides all splats so that they have unit
	 * luminance (though it leaves the \c luminance field untouched).
	 * When given an optional importance map in 2-stage MLT approaches,
	 * it divides the splat values by the associated importance
	 * map values
	 */
	void normalize(const Bitmap *importanceMap = NULL);

	/// Return a string representation
	std::string toString() const;
};

MTS_NAMESPACE_END

#endif /* __MITSUBA_BIDIR_PATHSAMPLER_H_ */
