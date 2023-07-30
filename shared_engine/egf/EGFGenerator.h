//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Equilibrium Graphics File script compler and generator
//////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "EGFPhysicsGenerator.h"

namespace SharedModel
{
	struct dsmmodel_t;
	struct dsmskelbone_t;
	struct dsmgroup_t;
	struct dsmvertex_t;
	struct dsmweight_t;

	struct esmshapedata_t;
	struct esmshapekey_t;
};

class IVirtualStream;

//
// EGF model generator (EDITOR-friendly)
//
class CEGFGenerator
{
public:

	CEGFGenerator();
	virtual ~CEGFGenerator();

	bool		InitFromKeyValues(const char* filename);
	bool		InitFromKeyValues(const KVSection* kvs);
	void		Cleanup();

	void		SetRefsPath(const char* path);
	void		SetOutputFilename(const char* filename);

	bool		GenerateEGF();
	bool		GeneratePOD();

protected:
	struct GenLODList_t;
	struct GenModel_t;
	struct GenIKChain_t;
	struct GenIKLink_t;
	struct GenBone_t;
	struct GenMaterialDesc_t;
	struct GenMaterialGroup_t;

	// helper functions
	GenBone_t*				FindBoneByName(const char* pszName) const;
	GenLODList_t*			FindModelLodGroupByName(const char* pszName) const;
	int						FindModelIndexByName(const char* pszName) const;
	GenModel_t*				FindModelByName(const char* pszName) const;

	int						FindModelLodIdGroupByName(const char* pszName) const;
	int						GetMaterialIndex(const char* pszName) const;

	// loader functions
	bool					LoadModel(const char* pszFileName, GenModel_t& mod);
	void					FreeModel(GenModel_t& mod );
	bool					PostProcessDSM(GenModel_t& mod );

	void					LoadModelsFromFBX(const KVSection* pKeyBase);
	int						ParseAndLoadModels(const KVSection* pKeyBase);

	bool					ParseModels(const KVSection* pSection);
	void					ParseLodData(const KVSection* pSection, int lodIdx);
	void					ParseLods(const KVSection* pSection);
	bool					ParseBodyGroups(const KVSection* pSection);
	bool					ParseMaterialGroups(const KVSection* pSection);
	bool					ParseMaterialPaths(const KVSection* pSection);
	bool					ParseMotionPackagePaths(const KVSection* pSection);
	void					ParseIKChain(const KVSection* pSection);
	void					ParseIKChains(const KVSection* pSection);
	void					ParseAttachments(const KVSection* pSection);
	void					ParsePhysModels(const KVSection* pSection);

	void					AddModelLodUsageReference(int modelLodIndex);

	// preprocessing
	void					MergeBones();
	void					BuildBoneChains();

	int						UsedMaterialIndex(const char* pszName);

	// writing to stream	
	void					WriteGroup(studiohdr_t* header, IVirtualStream* stream, SharedModel::dsmgroup_t* srcGroup, SharedModel::esmshapekey_t* modShapeKey, modelgroupdesc_t* dstGroup);

	void					WriteModels(studiohdr_t* header, IVirtualStream* stream);
	void					WriteLods(studiohdr_t* header, IVirtualStream* stream);
	void					WriteBodyGroups(studiohdr_t* header, IVirtualStream* stream);
	void					WriteAttachments(studiohdr_t* header, IVirtualStream* stream);
	void					WriteIkChains(studiohdr_t* header, IVirtualStream* stream);
	void					WriteMaterialDescs(studiohdr_t* header, IVirtualStream* stream);
	void					WriteMaterialPaths(studiohdr_t* header, IVirtualStream* stream);
	void					WriteMotionPackageList(studiohdr_t* header, IVirtualStream* stream);
	void					WriteBones(studiohdr_t* header, IVirtualStream* stream);

	void					Validate(studiohdr_t* header, const char* stage);

	// data
	Array<GenModel_t>				m_modelrefs{ PP_SL };		// all loaded model references

	Array<GenLODList_t>				m_modelLodLists{ PP_SL };	// all LOD reference models including main LOD
	Array<studiolodparams_t>		m_lodparams{ PP_SL };		// lod parameters
	Array<motionpackagedesc_t>		m_motionpacks{ PP_SL };		// motion packages
	Array<materialpathdesc_t>		m_matpathes{ PP_SL };		// material paths
	Array<GenIKChain_t>				m_ikchains{ PP_SL };		// ik chain list
	Array<GenBone_t>				m_bones{ PP_SL };			// bone list
	Array<studioattachment_t>		m_attachments{ PP_SL };		// attachment list
	Array<studiobodygroup_t>		m_bodygroups{ PP_SL };		// body group list
	Array<GenMaterialDesc_t>		m_materials{ PP_SL };		// materials that referenced by models

	// only participates in write
	Array<GenMaterialDesc_t*>		m_usedMaterials{ PP_SL };	// materials that used by models referenced by body groups
	Array<GenMaterialGroup_t*>		m_matGroups{ PP_SL };		// material groups

	// settings
	Vector3D						m_modelScale{ 1.0f };
	Vector3D						m_modelOffset{ 0.0f };
	bool							m_notextures{ false };

	EqString						m_refsPath;
	EqString						m_outputFilename;

	CEGFPhysicsGenerator			m_physModels;
};

struct CEGFGenerator::GenBone_t
{
	SharedModel::dsmskelbone_t* refBone{ nullptr };

	Array<GenBone_t*>	childs{ PP_SL };
	GenBone_t* parent{ nullptr };
};

struct CEGFGenerator::GenIKLink_t
{
	Vector3D	mins;
	Vector3D	maxs;

	CEGFGenerator::GenBone_t* bone;

	float		damping;
};

struct CEGFGenerator::GenIKChain_t
{
	char name[44]{ 0 };
	Array<GenIKLink_t> link_list{ PP_SL };
};

struct CEGFGenerator::GenModel_t
{
	EqString							name;

	CRefPtr<SharedModel::dsmmodel_t>		model{ nullptr };

	// bake data
	CRefPtr<SharedModel::esmshapedata_t>	shapeData{ nullptr };

	int								shapeIndex{ -1 };
	int								used{ 0 };
};

struct CEGFGenerator::GenLODList_t
{
	FixedArray<int, MAX_MODEL_LODS>	lodmodels;
	EqString						name;
};

struct CEGFGenerator::GenMaterialDesc_t
{
	char				materialname[32]{ 0 };
	int					used{ 0 };
};

struct CEGFGenerator::GenMaterialGroup_t
{
	Array<GenMaterialDesc_t> materials{ PP_SL };
};