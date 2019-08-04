//////////////////////////////////////////////////////////////////////////////////
// Copyright � Inspiration Byte
// 2009-2015
//////////////////////////////////////////////////////////////////////////////////
// Description: Heightfield
//////////////////////////////////////////////////////////////////////////////////

#include "world.h"

#include "heightfield.h"

#include "materialsystem/MeshBuilder.h"

#include "utils/VirtualStream.h"
#include "IDebugOverlay.h"
#include "math/math_util.h"

CHeightTileField::CHeightTileField()
{
	m_position = vec3_zero;
	m_points = NULL;

	m_hasTransparentSubsets = false;

	m_sizew = 0;
	m_sizeh = 0;

	m_regionPos = 0;

	m_fieldIdx = 0;
	m_levOffset = 0;

	m_physData = NULL;
}

CHeightTileField::~CHeightTileField()
{
	Destroy();
}

void CHeightTileField::Init(int size, const IVector2D& regionPos)
{
	m_sizew = size;
	m_sizeh = size;

	m_regionPos = regionPos;

#ifdef EDITOR
	m_points = new hfieldtile_t[m_sizew*m_sizeh];
#endif // EDITOR
}

void CHeightTileField::Destroy()
{
	delete [] m_points;
	m_points = NULL;

	UnloadMaterials();
}

void CHeightTileField::UnloadMaterials()
{
	for(int i = 0; i < m_materials.numElem(); i++)
	{
		materials->FreeMaterial( m_materials[i]->material );
		delete m_materials[i];
	}
	m_materials.clear();
	m_hasTransparentSubsets = false;
}

struct material_s
{
	char materialname[260];
};

ALIGNED_TYPE(material_s,4) material_t;

void CHeightTileField::ReadOnlyPoints( IVirtualStream* stream )
{
	if(!m_points)
		m_points = new hfieldtile_t[m_sizew*m_sizeh];

	stream->Read(m_points, m_sizew*m_sizeh, sizeof(hfieldtile_t));

	int numMaterials = 0;
	int matNamesSize = 0;

	stream->Read(&numMaterials, 1, sizeof(int));
	stream->Read(&matNamesSize, 1, sizeof(int));

	stream->Seek(matNamesSize, VS_SEEK_CUR);
}

void CHeightTileField::ReadOnlyMaterials( IVirtualStream* stream )
{
	if(m_materials.numElem() > 0)
		return;

	stream->Seek(m_sizew*m_sizeh*sizeof(hfieldtile_t), VS_SEEK_CUR);

	int numMaterials = 0;
	int matNamesSize = 0;

	stream->Read(&numMaterials, 1, sizeof(int));
	stream->Read(&matNamesSize, 1, sizeof(int));

	char* matNamesData = new char[matNamesSize];

	stream->Read(matNamesData, 1, matNamesSize);

	char* matNamePtr = matNamesData;

	for(int i = 0; i < numMaterials; i++)
	{
		hfieldmaterial_t* matBundle = new hfieldmaterial_t();
		matBundle->material = materials->GetMaterial(matNamePtr);
		matBundle->atlas = TexAtlas_LoadAtlas((materials->GetMaterialPath() + _Es(CORRECT_PATH_SEPARATOR) +_Es(matNamePtr)+".atlas").c_str(), matNamePtr, true);

		if(matBundle->material)
			matBundle->material->Ref_Grab();

		m_materials.append( matBundle );

		// valid?
		matNamePtr += strlen(matNamePtr)+1;
	}

	delete [] matNamesData;
}

void CHeightTileField::ReadFromStream( IVirtualStream* stream )
{
	if(!m_points)
		m_points = new hfieldtile_t[m_sizew*m_sizeh];

	stream->Read(m_points, m_sizew*m_sizeh, sizeof(hfieldtile_t));

	int numMaterials = 0;
	int matNamesSize = 0;

	stream->Read(&numMaterials, 1, sizeof(int));
	stream->Read(&matNamesSize, 1, sizeof(int));

	char* matNamesData = new char[matNamesSize];

	stream->Read(matNamesData, 1, matNamesSize);

	char* matNamePtr = matNamesData;

	for(int i = 0; i < numMaterials; i++)
	{
		hfieldmaterial_t* matBundle = new hfieldmaterial_t();
		matBundle->material = materials->GetMaterial(matNamePtr);
		matBundle->atlas = TexAtlas_LoadAtlas((materials->GetMaterialPath() + _Es(CORRECT_PATH_SEPARATOR) +_Es(matNamePtr)+".atlas").c_str(), matNamePtr, true);

		if(matBundle->material)
		{
			matBundle->material->Ref_Grab();

			if(matBundle->material->GetFlags() & MATERIAL_FLAG_TRANSPARENT)
				m_hasTransparentSubsets = true;
		}

		m_materials.append( matBundle );

		// valid?
		matNamePtr += strlen(matNamePtr)+1;
	}

	delete [] matNamesData;
}

bool CHeightTileField::IsEmpty()
{
	for(int x = 0; x < m_sizew; x++)
	{
		for(int y = 0; y < m_sizeh; y++)
		{
			if(m_points[y*m_sizew+x].texture != -1)
				return false;
		}
	}

	return true;
}

#ifdef EDITOR
void CHeightTileField::FreeUnusedMaterials()
{
	DkList<hfieldmaterial_t*> usedMaterials;

	for(int i = 0; i < m_materials.numElem(); i++)
		m_materials[i]->used = false;
	
	// mark materials used or not
	// and remap the cell materials
	for(int y = 0; y < m_sizeh; y++)
	{
		for(int x = 0; x < m_sizew; x++)
		{
			int matId = m_points[y*m_sizew+x].texture;

			if(matId == -1)
				continue;

			int newTexId = usedMaterials.addUnique( m_materials[matId] );
			m_materials[matId]->used = true;

			m_points[y*m_sizew+x].texture = newTexId;
		}
	}

	// free unused materials
	for(int i = 0; i < m_materials.numElem(); i++)
	{
		if(m_materials[i]->used)
			continue;

		materials->FreeMaterial(m_materials[i]->material);
		delete m_materials[i];
	}

	m_materials.clear();
	m_materials.append( usedMaterials);
}
#endif // EDITOR

int CHeightTileField::WriteToStream( IVirtualStream* stream )
{
#ifdef EDITOR
	// first of all we freeing unused materials
	FreeUnusedMaterials();
#endif // EDITOR

	long fpos = stream->Tell();

	// write heightfield data
	stream->Write(m_points, m_sizew*m_sizeh, sizeof(hfieldtile_t));

	// write material names
	{
		int numMaterials = m_materials.numElem();

		char nullSymbol = '\0';

		// write model names
		CMemoryStream matNamesData;
		matNamesData.Open(NULL, VS_OPEN_WRITE, 2048);

		for(int i = 0; i < numMaterials; i++)
		{
			matNamesData.Print( m_materials[i]->material->GetName() );
			matNamesData.Write( &nullSymbol, 1, 1 );
		}

		matNamesData.Write(&nullSymbol, 1, 1);

		int matNamesSize = matNamesData.Tell();

		stream->Write(&numMaterials, 1, sizeof(int));
		stream->Write(&matNamesSize, 1, sizeof(int));

		stream->Write(matNamesData.GetBasePointer(), 1, matNamesSize);
	}

	// return written byte count
	return stream->Tell() - fpos;
}

// optimizes heightfield, removing unused cells
void CHeightTileField::Optimize()
{
	// refresh size, position

	// GenerateRenderData();
}

// get/set
bool CHeightTileField::SetPointMaterial(int x, int y, IMaterial* pMaterial, int atlIdx)
{
	if(	(x >= m_sizew || y >= m_sizeh) ||
		(x < 0 || y < 0))
		return false;

	int tileIdx = y*m_sizew + x;
	hfieldtile_t& tile = m_points[tileIdx];

	if(pMaterial == NULL)
	{
		//int matIdx = tile.texture;

		// decrement
		//if(matIdx >= 0)
		//	m_materials[matIdx]->material->Ref_Drop();

		tile.texture = -1;
	}
	else
	{
		int matIdx = -1;
		for(int i = 0; i < m_materials.numElem(); i++)
		{
			if(m_materials[i]->material == pMaterial)
			{
				matIdx = i;
				break;
			}
		}

		if(matIdx == -1)
		{
			// try to load atlas
			hfieldmaterial_t* matBundle = new hfieldmaterial_t;
			matBundle->material = pMaterial;
			matBundle->atlas = TexAtlas_LoadAtlas((materials->GetMaterialPath() + _Es(CORRECT_PATH_SEPARATOR)+_Es(pMaterial->GetName())+".atlas").c_str(), pMaterial->GetName(), true);

			matIdx = m_materials.append(matBundle);

			if(matBundle->material->GetFlags() & MATERIAL_FLAG_TRANSPARENT)
				m_hasTransparentSubsets = true;
		}

		if(tile.texture == matIdx && tile.atlasIdx == atlIdx)
			return false;

		// decrement
		//if(tile.texture >= 0)
		//	m_materials[tile.texture]->material->Ref_Drop();

		// increment
		tile.texture = matIdx;
		tile.atlasIdx = atlIdx;

		pMaterial->Ref_Grab();
	}

	return true;
}

// returns tile for modifying
hfieldtile_t* CHeightTileField::GetTile(int x, int y) const
{
	// ������� ������
	CHeightTileField* neighbour = NULL;

	return GetTileAndNeighbourField(x, y, &neighbour);
}

void UTIL_GetTileIndexes(const IVector2D& tileXY, const IVector2D& fieldWideTall, IVector2D& outTileXY, IVector2D& outFieldOffset)
{
	// if we're out of bounds - try to find neightbour tile
	if(	(tileXY.x >= fieldWideTall.x || tileXY.y >= fieldWideTall.y) ||
		(tileXY.x < 0 || tileXY.y < 0))
	{
		// only -1/+1, no more
		int ofs_x = (tileXY.x < 0) ? -1 : ((tileXY.x >= fieldWideTall.x) ? 1 : 0 );
		int ofs_y = (tileXY.y < 0) ? -1 : ((tileXY.y >= fieldWideTall.y) ? 1 : 0 );

		outFieldOffset.x = ofs_x;
		outFieldOffset.y = ofs_y;

		// ������� ������
		// rolling
		outTileXY.x = ROLLING_VALUE(tileXY.x, fieldWideTall.x);
		outTileXY.y = ROLLING_VALUE(tileXY.y, fieldWideTall.y);

		return;
	}

	outFieldOffset.x = 0;
	outFieldOffset.y = 0;

	outTileXY = tileXY;
}

hfieldtile_t* CHeightTileField::GetTileAndNeighbourField(int x, int y, CHeightTileField** field) const
{
	// if we're out of bounds - try to find neightbour tile
	if(	(x >= m_sizew || y >= m_sizeh) ||
		(x < 0 || y < 0))
	{
		if(m_regionPos.x < 0)
			return NULL;

		// only -1/+1, no more
		int ofs_x = (x < 0) ? -1 : ((x >= m_sizew) ? 1 : 0 );
		int ofs_y = (y < 0) ? -1 : ((y >= m_sizeh) ? 1 : 0 );

		// ������� ������
		*field = g_pGameWorld->m_level.GetHeightFieldAt( IVector2D(m_regionPos.x + ofs_x, m_regionPos.y + ofs_y), m_fieldIdx );

		if(*field)
		{
			// rolling
			int tofs_x = ROLLING_VALUE(x, (*field)->m_sizew);
			int tofs_y = ROLLING_VALUE(y, (*field)->m_sizeh);

			return (*field)->GetTile(tofs_x, tofs_y);
		}
		else
			return NULL;
	}

	return &m_points[y*m_sizew + x];
}

void CHeightTileField::GetTileTBN(int x, int y, Vector3D& tang, Vector3D& binorm, Vector3D& norm) const
{
	int dx[] = NEIGHBOR_OFFS_XDX(x, 1);
	int dy[] = NEIGHBOR_OFFS_YDY(y, 1);

	hfieldtile_t* tile = GetTile(x,y);
	Vector3D tilePosition(x*HFIELD_POINT_SIZE, (float)tile->height*HFIELD_HEIGHT_STEP, y*HFIELD_POINT_SIZE);

	Vector3D t(0,1,1);
	Vector3D b(1,1,0);

	// tangent and binormal, positive and negative
	Vector3D tp(0),tn(0);
	Vector3D bp(0),bn(0);

	int nIter = 0;

	bool isDetached = (tile->flags & EHTILE_DETACHED) > 0;

	// get neighbour tiles
	for (int i = 0; i < 8; i++)
	{
		// get the tiles only with corresponding detaching
		hfieldtile_t* ntile = GetTile(dx[i], dy[i]);

		if (!ntile)
			continue;

		bool isNDetached = (ntile->flags & EHTILE_DETACHED) > 0;

		if (isDetached != isNDetached &&
			ntile->height != tile->height)
			continue;

		if(ntile->texture == -1)
			continue;

		Vector3D ntilePosition(dx[i]*HFIELD_POINT_SIZE, (float)ntile->height*HFIELD_HEIGHT_STEP, dy[i]*HFIELD_POINT_SIZE);

		// make only y has sign
		Vector3D tt = (ntilePosition-tilePosition)*t;
		Vector3D bb = (ntilePosition-tilePosition)*b;

		float ttd = dot(vec3_forward, tt);
		float bbd = dot(vec3_right, bb);

		if(ttd > 0)
			tp += Vector3D(0.0f, tt.y, ttd);
		else
			tn += Vector3D(0.0f, tt.y, ttd);

		if(bbd > 0)
			bp += Vector3D(bbd, bb.y, 0.0f);
		else
			bn += Vector3D(bbd, bb.y, 0.0f);

		//tp.y += tt.y;
		//bp.y += bb.y;

		nIter++;
	}

	// single tile island?
	if(nIter <= 2)
	{
		tang = Vector3D(0.0f, 0.0f, 1.0f);
		binorm = Vector3D(1.0f, 0.0f, 0.0f);
		norm = Vector3D(0.0f, 1.0f, 0.0f);

		return;
	}

	tang = tp-tn;
	binorm = bp-bn;

	if(lengthSqr(tang) <= 0.01f)
		tang = Vector3D(0.0f, 0.0f, 1.0f);

	if(lengthSqr(binorm) <= 0.01f)
		binorm = Vector3D(1.0f, 0.0f, 0.0f);

	tang = fastNormalize(tang);
	binorm = fastNormalize(binorm);
	norm = cross(tang, binorm);

	/*
	{
		debugoverlay->Line3D(m_position+tilePosition, m_position+tilePosition+tang, ColorRGBA(1,0,0,1), ColorRGBA(1,0,0,1), 0.0f );
		debugoverlay->Line3D(m_position+tilePosition, m_position+tilePosition+binorm, ColorRGBA(0,1,0,1), ColorRGBA(0,1,0,1), 0.0f );
		debugoverlay->Line3D(m_position+tilePosition, m_position+tilePosition+norm, ColorRGBA(0,0,1,1), ColorRGBA(0,0,1,1), 0.0f );
	}*/

	return;
}

hfieldtile_t* CHeightTileField::GetTile_CheckFlag(int x, int y, int flag, bool enabled) const
{
	hfieldtile_t* tile = GetTile(x,y);

	if(!tile)
		return NULL;

	if((tile->flags & flag) > 0 != enabled)
		return NULL;

	return tile;
}

// returns face at position
bool CHeightTileField::PointAtPos(const Vector3D& pos, int& x, int& y) const
{
	Vector3D zeroedPos = pos - m_position;

	float p_size = (1.0f / HFIELD_POINT_SIZE);

	Vector2D xz_pos = zeroedPos.xz() * p_size;

	x = xz_pos.x+0.5f;
	y = xz_pos.y+0.5f;

	if(x < 0 || x >= m_sizew)
		return false;

	if(y < 0 || y >= m_sizeh)
		return false;

	return true;
}

void EdgeIndexToVertex(int edge, int& vi1, int& vi2)
{
	int i1, i2;

	i1 = edge;
	i2 = edge-1;

	if(i1 < 0)
		i1 = 3;

	if(i2 < 0)
		i2 = 3;

	vi1 = i1;
	vi2 = i2;
}

hfieldbatch_t* FindBatchInList(IMaterial* pMaterial, DkList<hfieldbatch_t*>& batches, bool useSplitCoords, int sx, int sy)
{
	for(int i = 0; i < batches.numElem(); i++)
	{
		IMaterial* mat = batches[i]->materialBundle->material;

		if(useSplitCoords)
		{
			if(mat == pMaterial && batches[i]->sx == sx && batches[i]->sy == sy)
				return batches[i];
		}
		else if(mat == pMaterial)
			return batches[i];
	}

	return NULL;
}

int valid_edge_index(int idx)
{
	int eidx = idx;

	if(eidx > 2)
		eidx -= 3;

	if(eidx < 0)
		eidx += 3;

	return eidx;
}

bool hfieldVertexComparator(const hfielddrawvertex_t& a, const hfielddrawvertex_t& b)
{
	if(a.position == b.position && a.texcoord == b.texcoord)
		return true;

	return false;
}

void CHeightTileField::Generate(EHFieldGeometryGenerateMode mode, DkList<hfieldbatch_t*>& batches, float subdivision )
{
	Vector3D hfield_offset = (mode == HFIELD_GEOM_PHYSICS) ? vec3_zero : m_position;

	m_hasTransparentSubsets = false;

	float hfieldSizeW = m_sizew*HFIELD_POINT_SIZE;
	float hfieldSizeH = m_sizeh*HFIELD_POINT_SIZE;

	// generate polys
	for(int x = 0; x < m_sizew; x++)
	{
		for(int y = 0; y < m_sizeh; y++)
		{
			int pt_idx = y*m_sizew + x;
			hfieldtile_t& point = m_points[pt_idx];

			if( point.texture == -1 )
				continue;

			int sx = floor((float)x / subdivision);
			int sy = floor((float)y / subdivision);

			//Msg("tile=%d %d sxsy=%d %d\n", x,y, sx, sy);

			hfieldbatch_t* batch = FindBatchInList( m_materials[point.texture]->material, batches, /*generate_render*/true, sx, sy);

			float fTexelX = 0.0f;
			float fTexelY = 0.0f;

			if(!batch)
			{
				int nBatchFlags = 0;

				IMaterial* material = m_materials[point.texture]->material;

				IMatVar* nocollide = material->FindMaterialVar("nocollide");
				IMatVar* detach = material->FindMaterialVar("detached");
				IMatVar* addwall = material->FindMaterialVar("addwall");
				IMatVar* rotatable = material->FindMaterialVar("rotatable");

				if(nocollide)
					nBatchFlags |= nocollide->GetInt() ? (EHTILE_NOCOLLIDE) : 0;

				if(detach)
					nBatchFlags |= detach->GetInt() ? EHTILE_DETACHED : 0;

				if(addwall)
					nBatchFlags |= addwall->GetInt() ? EHTILE_ADDWALL : 0;

				if(rotatable)
					nBatchFlags |= rotatable->GetInt() ? EHTILE_ROTATABLE : 0;

				if((mode == HFIELD_GEOM_PHYSICS) && (nBatchFlags & EHTILE_NOCOLLIDE))
					continue;

				batch = new hfieldbatch_t;
				batch->materialBundle = m_materials[point.texture];
				batch->verts.resize(m_sizew*m_sizeh*6);
				batch->indices.resize(m_sizew*m_sizeh*6);
				batch->flags = nBatchFlags;

				batch->sx = sx;
				batch->sy = sy;

				if(batch->materialBundle->material->GetFlags() & MATERIAL_FLAG_TRANSPARENT)
					m_hasTransparentSubsets = true;

				batches.append(batch);
			}

			CTextureAtlas* batchAtlas = batch->materialBundle->atlas;

			/*
			IMaterial* batchMaterial = batch->materialBundle->material;
			if(batchMaterial->GetBaseTexture())
			{
				fTexelX = 1.0f / batchMaterial->GetBaseTexture()->GetWidth();
				fTexelY = 1.0f / batchMaterial->GetBaseTexture()->GetHeight();
			}*/

			int vertex_heights[4] = {point.height, point.height, point.height, point.height};

			int xv[4] = NEIGHBOR_OFFS_X(x);
			int yv[4] = NEIGHBOR_OFFS_Y(y);

			int xvd[4] = NEIGHBOR_OFFS_DX(x, 1);
			int yvd[4] = NEIGHBOR_OFFS_DY(y, 1);

			int pointFlags = (point.flags | batch->flags);

			bool isDetached = (pointFlags & EHTILE_DETACHED) > 0;
			bool addWallOnEdges = ((pointFlags & EHTILE_ADDWALL) && (mode != HFIELD_GEOM_PHYSICS)) || ((pointFlags & EHTILE_ADDWALL) && (pointFlags & EHTILE_COLLIDE_WALL));
			bool isEmpty = (pointFlags & EHTILE_EMPTY) > 0;
			bool rotatable = (pointFlags & EHTILE_ROTATABLE) > 0;

			if((mode == HFIELD_GEOM_PHYSICS) && (pointFlags & EHTILE_NOCOLLIDE))
				continue;

			bool verts_stripped[4] = { false, false, false, false };
			bool edges_stripped[4] = {false, false, false, false};
			bool edges_wall[4] = { false, false, false, false };
			int  edge_stripped_height[4] = {0,0,0,0};

			// ����������� ������ ������� ����� �� ������
			for(int i = 0; i < 4; i++)
			{
				//GetTile(xv[i], yv[i]);
				hfieldtile_t* ntile = GetTile(xv[i], yv[i]);//GetTile_CheckFlag(xv[i], yv[i], EHTILE_DETACHED, isDetached);

				int v1, v2;
				EdgeIndexToVertex(i, v1, v2);

				// ������� � ������� �� ������
				if (ntile && ntile->texture != -1 && ((ntile->flags & EHTILE_DETACHED) > 0) == isDetached)
				{
					if( ntile->height > vertex_heights[v1] )
						vertex_heights[v1] = ntile->height;

					if( ntile->height > vertex_heights[v2] )
						vertex_heights[v2] = ntile->height;
				}
				else if(ntile)
				{
					verts_stripped[v1] = (ntile->height > vertex_heights[v1]);
					verts_stripped[v2] = (ntile->height > vertex_heights[v2]);
				}
				else
				{
					verts_stripped[v1] = true;
					verts_stripped[v2] = true;
				}

				ntile = GetTile(xvd[i], yvd[i]);//GetTile_CheckFlag(xvd[i], yvd[i], EHTILE_DETACHED, isDetached);

				// ����� �� ������� �������
				if (ntile && ntile->texture != -1 && ((ntile->flags & EHTILE_DETACHED) > 0) == isDetached)
				{
					if( ntile->height > vertex_heights[i] )
						vertex_heights[i] = ntile->height;
				}
				else if(ntile)
					verts_stripped[i] = (ntile->height > vertex_heights[i]);
				else
					verts_stripped[i] = true;
			}

			// ����������, ����� �� �����
			for(int i = 0; i < 4; i++)
			{
				int i1, i2;

				i1 = valid_edge_index(i-1);
				i2 = valid_edge_index(i+1);

				int edge_ngb[] = {i1, i2};

				hfieldtile_t* ntile = GetTile_CheckFlag(xv[i], yv[i], EHTILE_DETACHED, !isDetached);

				if(ntile && isDetached != ((ntile->flags & EHTILE_DETACHED) > 0) /*&& ntile->height < vertex_heights[i]*/)
				{
					edges_stripped[i] = true;
					edges_wall[i] = addWallOnEdges;
					edge_stripped_height[i] = ntile->height;

					for(int j = 0; j < 2; j++)
					{
						hfieldtile_t* ntile2 = GetTile_CheckFlag(xv[edge_ngb[j]], yv[edge_ngb[j]], EHTILE_DETACHED, !isDetached);

						// ��� ����� �� ���������� � ������� � ������ (���� ��-���� ���-�� �����)
						if(ntile2 && isDetached != ((ntile2->flags & EHTILE_DETACHED) > 0))
						{
							edges_stripped[edge_ngb[j]] = true;
							edges_wall[i] = addWallOnEdges;
						}
					}
				}
			}

			// ���������� ��� �������

			float dxv[4] = NEIGHBOR_OFFS_DX(float(x), 0.5f);
			float dyv[4] = NEIGHBOR_OFFS_DY(float(y), 0.5f);
			float drxv[4] = NEIGHBOR_OFFS_DX(0.0f, 0.5f);
			float dryv[4] = NEIGHBOR_OFFS_DY(0.0f, 0.5f);

			int vindxs[4];

			if(!isEmpty || addWallOnEdges)
			{
				for(int i = 0; i < 4; i++)
				{
					Vector3D point_position(dxv[i] * HFIELD_POINT_SIZE, float(vertex_heights[i])*HFIELD_HEIGHT_STEP, dyv[i] * HFIELD_POINT_SIZE);

					int rIndex = rotatable ? (i + point.rotatetex) : i;

					if(rIndex > 3)
						rIndex -= 4;

					float tc_x = 0;
					float tc_y = 0;

					if(mode == HFIELD_GEOM_RENDER)
					{
						if( batchAtlas )
						{
							TexAtlasEntry_t* atlEntry = batchAtlas->GetEntry(point.atlasIdx);

							Vector2D size = atlEntry->rect.GetSize();
							Vector2D center = atlEntry->rect.GetCenter();

							Vector2D tcd(drxv[rIndex],dryv[rIndex]);

							tcd = center + tcd*size;

							tc_x = tcd.x + fTexelX*0.5f;
							tc_y = tcd.y + fTexelY*0.5f;
						}
						else
						{
							if(rotatable)
							{
								tc_x = (drxv[rIndex] + 0.5f) + fTexelX*0.5f;
								tc_y = (dryv[rIndex] + 0.5f) + fTexelY*0.5f;
							}
							else
							{
								tc_x = dxv[rIndex] + 0.5f;
								tc_y = dyv[rIndex] + 0.5f;
							}
						}
					}
					else if(mode == HFIELD_GEOM_DEBUG)
					{
						tc_x = (point_position.x + HFIELD_POINT_SIZE*0.5f) / hfieldSizeW;
						tc_y = (point_position.z + HFIELD_POINT_SIZE*0.5f) / hfieldSizeH;
					}

					Vector2D texCoord = Vector2D(tc_x,tc_y);

					hfielddrawvertex_t vert(point_position + hfield_offset, Vector3D(0.0f, 1.0f, 0.0f), texCoord);

					vindxs[i] = batch->verts.addUnique(vert,hfieldVertexComparator);
					batch->bbox.AddVertex(vert.position);
				}

				if(!isEmpty)
				{

					//Vector3D norm1 = NormalOfTriangle(	batch->verts[vindxs[2]].position,
					//									batch->verts[vindxs[1]].position,
					//									batch->verts[vindxs[0]].position);

					if(mode != HFIELD_GEOM_PHYSICS)
					{
						Vector3D t,b,n;
						GetTileTBN( x, y, t,b,n );

						batch->verts[vindxs[0]].normal = n;
						batch->verts[vindxs[1]].normal = n;
						batch->verts[vindxs[2]].normal = n;
						batch->verts[vindxs[3]].normal = n;
					}

					// add quad
					batch->indices.append(vindxs[2]);
					batch->indices.append(vindxs[1]);
					batch->indices.append(vindxs[0]);

					batch->indices.append(vindxs[3]);
					batch->indices.append(vindxs[2]);
					batch->indices.append(vindxs[0]);
				}

				for(int i = 0; i < 4; i++)
				{
					int txv[4] = NEIGHBOR_OFFS_X(0);
					int tyv[4] = NEIGHBOR_OFFS_Y(0);

					int eindxs[4] = {-1,-1,-1,-1};

					if( edges_stripped[i] && edges_wall[i] )
					{
						int v1, v2;
						EdgeIndexToVertex(i, v1, v2);

						if(mode != HFIELD_GEOM_PHYSICS)
						{
							eindxs[0] = batch->verts.append( batch->verts[vindxs[v1]] );
							eindxs[1] = batch->verts.append( batch->verts[vindxs[v2]] );
						}
						else
						{
							eindxs[0] = vindxs[v1];
							eindxs[1] = vindxs[v2];
						}

						Vector3D point_position1(dxv[v1] * HFIELD_POINT_SIZE, float(edge_stripped_height[i])*HFIELD_HEIGHT_STEP, dyv[v1] * HFIELD_POINT_SIZE);
						Vector3D point_position2(dxv[v2] * HFIELD_POINT_SIZE, float(edge_stripped_height[i])*HFIELD_HEIGHT_STEP, dyv[v2] * HFIELD_POINT_SIZE);

						float fTexY1 = (batch->verts[vindxs[v1]].position.y-point_position1.y) / HFIELD_POINT_SIZE;//point_position1.y / HFIELD_POINT_SIZE;
						float fTexY2 = (batch->verts[vindxs[v2]].position.y-point_position2.y) / HFIELD_POINT_SIZE;//point_position2.y / HFIELD_POINT_SIZE;

						int rIndex = rotatable ? (i + point.rotatetex) : i;

						if(rIndex > 3)
							rIndex -= 4;

						int tv1, tv2;
						EdgeIndexToVertex(rIndex, tv1, tv2);

						// edge direction by texcoord
						Vector2D edgeTexDir(txv[rIndex], tyv[rIndex]);

						Vector2D texCoord1, texCoord2;

						if(rotatable)
						{
							texCoord1 = Vector2D(drxv[tv1]+0.5f, dryv[tv1]+0.5f) + edgeTexDir*fTexY1 + fTexelX*0.5f;
							texCoord2 = Vector2D(drxv[tv2]+0.5f, dryv[tv2]+0.5f) + edgeTexDir*fTexY2 + fTexelY*0.5f;
						}
						else
						{
							texCoord1 = Vector2D(dxv[tv1]+0.5f, dyv[tv1]+0.5f) + edgeTexDir*fTexY1 + fTexelX*0.5f;
							texCoord2 = Vector2D(dxv[tv2]+0.5f, dyv[tv2]+0.5f) + edgeTexDir*fTexY2 + fTexelY*0.5f;
						}

						hfielddrawvertex_t vert1 = hfielddrawvertex_t(point_position2 + hfield_offset, Vector3D(0, 1, 0), texCoord2);
						hfielddrawvertex_t vert2 =	hfielddrawvertex_t(point_position1 + hfield_offset, Vector3D(0, 1, 0), texCoord1);


						batch->bbox.AddVertex(vert1.position);
						batch->bbox.AddVertex(vert2.position);

						eindxs[2] = batch->verts.append(vert1);
						eindxs[3] = batch->verts.append(vert2);

						Vector3D norm1;

						float fCheckDegenerareArea1 = TriangleArea(	batch->verts[eindxs[2]].position,
																	batch->verts[eindxs[1]].position,
																	batch->verts[eindxs[0]].position);
						// invert normal to make it good
						if( fCheckDegenerareArea1 > 0.001f )
							norm1 = NormalOfTriangle(	batch->verts[eindxs[2]].position,
														batch->verts[eindxs[1]].position,
														batch->verts[eindxs[0]].position);
						else
							norm1 = NormalOfTriangle(	batch->verts[eindxs[3]].position,
														batch->verts[eindxs[2]].position,
														batch->verts[eindxs[0]].position);

						// FIXME: don't add degenerate triangles to physics
						// or it will make NaN issue (and ASSERT occur in CEqRigidBody::AccumulateForces)

						batch->verts[eindxs[0]].normal = norm1;
						batch->verts[eindxs[1]].normal = norm1;
						batch->verts[eindxs[2]].normal = norm1;
						batch->verts[eindxs[3]].normal = norm1;

						// add quad
						batch->indices.append(eindxs[2]);
						batch->indices.append(eindxs[1]);
						batch->indices.append(eindxs[0]);

						batch->indices.append(eindxs[3]);
						batch->indices.append(eindxs[2]);
						batch->indices.append(eindxs[0]);
					}
				}
			}
		}
	}

	// check the batches and remove if empty
	for (int i = 0; i < batches.numElem(); i++)
	{
		// validate batch
		if (batches[i]->indices.numElem() == 0)
		{
			delete batches[i];
			batches.removeIndex(i);
			i--;
		}
	}
}

void ListQuad(const Vector3D &v1, const Vector3D &v2, const Vector3D& v3, const Vector3D& v4, const ColorRGBA &color, DkList<Vertex3D_t> &verts)
{
	verts.append(Vertex3D_t(v3, vec2_zero, color));
	verts.append(Vertex3D_t(v2, vec2_zero, color));
	verts.append(Vertex3D_t(v1, vec2_zero, color));

	verts.append(Vertex3D_t(v4, vec2_zero, color));
	verts.append(Vertex3D_t(v3, vec2_zero, color));
	verts.append(Vertex3D_t(v1, vec2_zero, color));
}

void DrawGridH(int size, int count, const Vector3D& pos, const ColorRGBA &color, bool for2D)
{
	int grid_lines = count;

	g_pShaderAPI->SetTexture(NULL,NULL, 0);
	materials->SetDepthStates(!for2D,!for2D);
	materials->SetRasterizerStates(CULL_BACK,FILL_SOLID);
	materials->SetBlendingStates(BLENDFACTOR_SRC_ALPHA, BLENDFACTOR_ONE_MINUS_SRC_ALPHA);

	materials->BindMaterial(materials->GetDefaultMaterial());

	CMeshBuilder meshBuilder(materials->GetDynamicMesh());
	meshBuilder.Begin(PRIM_LINES);

		for(int i = 0; i <= grid_lines / size;i++)
		{
			int max_grid_size = grid_lines;
			int grid_step = size*i;

			meshBuilder.Color4fv(color);

			meshBuilder.Line3fv(pos + Vector3D(0,0,grid_step),pos + Vector3D(max_grid_size,0,grid_step));
			meshBuilder.Line3fv(pos + Vector3D(grid_step,0,0),pos + Vector3D(grid_step,0,max_grid_size));

			meshBuilder.Line3fv(pos + Vector3D(0,0,-grid_step),pos + Vector3D(-max_grid_size,0,-grid_step));
			meshBuilder.Line3fv(pos + Vector3D(-grid_step,0,0),pos + Vector3D(-grid_step,0,-max_grid_size));

			// draw another part
			meshBuilder.Line3fv(pos + Vector3D(0,0,-grid_step),pos + Vector3D(max_grid_size,0,-grid_step));
			meshBuilder.Line3fv(pos + Vector3D(-grid_step,0,0),pos + Vector3D(-grid_step,0,max_grid_size));

			meshBuilder.Line3fv(pos + Vector3D(0,0,grid_step),pos + Vector3D(-max_grid_size,0,grid_step));
			meshBuilder.Line3fv(pos + Vector3D(grid_step,0,0),pos + Vector3D(grid_step,0,-max_grid_size));
		}

	meshBuilder.End();
}

void CHeightTileField::DebugRender(bool bDrawTiles, float gridHeight)
{
	if(!m_sizew || !m_sizeh)
		return;

	materials->SetAmbientColor(1.0f);

	Vector3D halfsize = Vector3D(HFIELD_POINT_SIZE, 0, HFIELD_POINT_SIZE)*0.5f;
	DrawGridH(HFIELD_POINT_SIZE, m_sizew*2, 
				m_position + Vector3D(m_sizew*HFIELD_POINT_SIZE*0.5f, gridHeight, m_sizew*HFIELD_POINT_SIZE*0.5f) - halfsize, 
				ColorRGBA(1,1,1,0.1), false);

	if(!bDrawTiles)
		return;

	g_pShaderAPI->SetTexture(NULL,NULL, 0);
	materials->SetDepthStates(true, false);
	materials->SetRasterizerStates(CULL_BACK,FILL_SOLID);
	materials->SetBlendingStates(BLENDFACTOR_SRC_ALPHA, BLENDFACTOR_ONE_MINUS_SRC_ALPHA);

	materials->BindMaterial(materials->GetDefaultMaterial());

	CMeshBuilder meshBuilder(materials->GetDynamicMesh());
	meshBuilder.Begin(PRIM_TRIANGLES);

	for(int x = 0; x < m_sizew; x++)
	{
		for(int y = 0; y < m_sizeh; y++)
		{
			float dxv[4] = NEIGHBOR_OFFS_DX((float)x, 0.5f);
			float dyv[4] = NEIGHBOR_OFFS_DY((float)y, 0.5f);

			int pt_idx = y*m_sizew + x;
			hfieldtile_t& tile = m_points[pt_idx];

			Vector3D p1(dxv[0] * HFIELD_POINT_SIZE, float(tile.height)*HFIELD_HEIGHT_STEP+0.1f, dyv[0] * HFIELD_POINT_SIZE);
			Vector3D p2(dxv[1] * HFIELD_POINT_SIZE, float(tile.height)*HFIELD_HEIGHT_STEP+0.1f, dyv[1] * HFIELD_POINT_SIZE);
			Vector3D p3(dxv[2] * HFIELD_POINT_SIZE, float(tile.height)*HFIELD_HEIGHT_STEP+0.1f, dyv[2] * HFIELD_POINT_SIZE);
			Vector3D p4(dxv[3] * HFIELD_POINT_SIZE, float(tile.height)*HFIELD_HEIGHT_STEP+0.1f, dyv[3] * HFIELD_POINT_SIZE);

			p1 += m_position;
			p2 += m_position;
			p3 += m_position;
			p4 += m_position;

			ColorRGBA tileColor(0,0,0,0.1);

			if(tile.texture != -1)
			{
				tileColor.x = (tile.flags & EHTILE_DETACHED) > 0 ? 0.0f : 1.0f;
				tileColor.y = (tile.flags & EHTILE_ADDWALL) > 0 ? ((tile.flags & EHTILE_COLLIDE_WALL) ? 0.25 : 0.0f) : 1.0f;
				tileColor.z = (tile.flags & EHTILE_NOCOLLIDE) > 0 ? 0.0f : 1.0f;
				tileColor.w = 0.5f;

				tileColor = color4_white-tileColor;
			}

			meshBuilder.Color4fv(tileColor);

			// in this order because it's counter-clockwise
			meshBuilder.TexturedQuad3(p4,p3,p1,p2, vec2_zero, vec2_zero,vec2_zero,vec2_zero);
		}
	}

	meshBuilder.End();
}

void CHeightTileField::GetDecalPolygons( decalprimitives_t& polys, occludingFrustum_t* frustum)
{
	// we're getting vertex data from physics here
	if(m_physData == NULL)
		return;

	for(int i = 0; i < m_physData->m_batches.numElem(); i++)
	{
		hfieldbatch_t* batch = m_physData->m_batches[i];

		IMaterial* mat = batch->materialBundle->material;

		if(mat && (mat->GetFlags() & polys.settings.avoidMaterialFlags))
			continue;

		BoundingBox bbox = batch->bbox;
		bbox.minPoint += m_position;
		bbox.maxPoint += m_position;

		if(!polys.settings.clipVolume.IsBoxInside(bbox.minPoint, bbox.maxPoint))
			continue;

		if(frustum && !frustum->IsBoxVisible(bbox))
			continue;

		for(int p = 0; p < batch->indices.numElem(); p += 3)
		{
			int i1 = batch->indices[p];
			int i2 = batch->indices[p+1];
			int i3 = batch->indices[p+2];

			// add position because physics polys are not moved
			Vector3D p1 = batch->physicsVerts[i1]+m_position;
			Vector3D p2 = batch->physicsVerts[i2]+m_position;
			Vector3D p3 = batch->physicsVerts[i3]+m_position;

			if(!polys.settings.clipVolume.IsTriangleInside(p1,p2,p3))
				continue;

			polys.AddTriangle(p1,p2,p3);
		}
	}
}

//-----------------------------------------------------------------------------

#ifdef EDITOR
CHeightTileFieldRenderable::CHeightTileFieldRenderable() : CHeightTileField(), CUndoableObject()
#else
CHeightTileFieldRenderable::CHeightTileFieldRenderable() : CHeightTileField()
#endif // EDITOR
{
	m_batches = NULL;
	m_numBatches = 0;

	m_format = NULL;
	m_vertexbuffer = NULL;
	m_indexbuffer = NULL;

	m_isChanged = true;

}

CHeightTileFieldRenderable::~CHeightTileFieldRenderable()
{
	CleanRenderData();
}

#ifdef EDITOR
bool CHeightTileFieldRenderable::Undoable_WriteObjectData( IVirtualStream* stream )
{
	WriteToStream(stream);
	return true;
}

void CHeightTileFieldRenderable::Undoable_ReadObjectData( IVirtualStream* stream )
{
	g_pShaderAPI->Reset(STATE_RESET_VBO);
	g_pShaderAPI->ApplyBuffers();

	CleanRenderData(false);
	Destroy();
	ReadFromStream( stream );

	m_isChanged = true;
}
#endif // EDITOR

void CHeightTileFieldRenderable::CleanRenderData(bool deleteVBO)
{
	delete [] m_batches;

	m_batches = NULL;

	m_numBatches = 0;

	if(deleteVBO)
	{
		if(m_vertexbuffer)
			g_pShaderAPI->DestroyVertexBuffer(m_vertexbuffer);

		m_vertexbuffer = NULL;

		if(m_format)
			g_pShaderAPI->DestroyVertexFormat(m_format);

		m_format = NULL;

		if(m_indexbuffer)
			g_pShaderAPI->DestroyIndexBuffer(m_indexbuffer);

		m_indexbuffer = NULL;

		UnloadMaterials();
	}

	m_isChanged = true;
}

void CHeightTileFieldRenderable::GenereateRenderData(bool debug)
{
	if(!m_isChanged)
		return;

	m_isChanged = false;

	// delete batches only
	CleanRenderData(false);

	DkList<hfieldbatch_t*> batches;

	// precache it first
	for(int i = 0; i < m_materials.numElem(); i++)
		materials->PutMaterialToLoadingQueue( m_materials[i]->material );

	// ���������, ���������� ����� ��������� � ������������ �� ����������
	Generate(debug ? HFIELD_GEOM_DEBUG : HFIELD_GEOM_RENDER, batches);

	if(batches.numElem() == 0)
	{
		m_numBatches = 0;
		return;
	}

	m_batches = new hfielddrawbatch_t[batches.numElem()];
	DkList<hfielddrawvertex_t>	verts;
	DkList<int>					indices;

	for(int i = 0; i < batches.numElem(); i++)
	{
		m_batches[i].startVertex = verts.numElem();
		m_batches[i].numVerts = batches[i]->verts.numElem();

		m_batches[i].startIndex = indices.numElem();
		m_batches[i].numIndices = batches[i]->indices.numElem();
		m_batches[i].pMaterial = batches[i]->materialBundle->material;
		//m_batches[i].pMaterial->Ref_Grab();

		for(int j = 0; j < batches[i]->verts.numElem(); j++)
		{
			m_batches[i].bbox.AddVertex( batches[i]->verts[j].position );
			//verts.append(batches[i]->verts[j]);
		}

		verts.append(batches[i]->verts);

		indices.resize(indices.numElem() + batches[i]->indices.numElem());

		for(int j = 0; j < batches[i]->indices.numElem(); j++)
			indices.append(batches[i]->indices[j] + m_batches[i].startVertex);

		// that' all, folks
		delete batches[i];
	}

	m_numVerts = verts.numElem();

	m_numBatches = batches.numElem();

	if(!m_vertexbuffer || !m_indexbuffer || !m_format)
	{
		VertexFormatDesc_t pFormat[] = {
			{ 0, 3, VERTEXATTRIB_POSITION, ATTRIBUTEFORMAT_FLOAT },	  // position
			{ 0, 2, VERTEXATTRIB_TEXCOORD, ATTRIBUTEFORMAT_HALF }, // texcoord 0
			{ 0, 4, VERTEXATTRIB_TEXCOORD, ATTRIBUTEFORMAT_HALF }, // Normal (TC1) + border
		};

		DevMsg(2,"Creating hfield buffers, %d verts %d indices in %d batches\n", verts.numElem(), indices.numElem(), m_numBatches);

		if(!m_format)
			m_format = g_pShaderAPI->CreateVertexFormat(pFormat, elementsOf(pFormat));

#ifdef EDITOR
		ER_BufferAccess bufferType = BUFFER_STATIC;

		int vb_lock_size = m_sizew*m_sizeh*12;
		int ib_lock_size = m_sizew*m_sizeh*16;

		m_vertexbuffer = g_pShaderAPI->CreateVertexBuffer(bufferType, vb_lock_size, sizeof(hfielddrawvertex_t), NULL);
		m_indexbuffer = g_pShaderAPI->CreateIndexBuffer(ib_lock_size, sizeof(int), bufferType, NULL);
#else
		ER_BufferAccess bufferType = BUFFER_STATIC;
		int vb_lock_size = verts.numElem();
		int ib_lock_size = indices.numElem();

		m_vertexbuffer = g_pShaderAPI->CreateVertexBuffer(bufferType, vb_lock_size, sizeof(hfielddrawvertex_t), verts.ptr());
		m_indexbuffer = g_pShaderAPI->CreateIndexBuffer(ib_lock_size, sizeof(int), bufferType, indices.ptr());
#endif
	}

#ifdef EDITOR
	m_vertexbuffer->Update(verts.ptr(), verts.numElem(), 0, true);
	m_indexbuffer->Update(indices.ptr(), indices.numElem(), 0, true);
#endif
}

ConVar r_drawHeightfields("r_drawHeightfields", "1", NULL, CV_CHEAT);

void CHeightTileFieldRenderable::Render(int nDrawFlags, const occludingFrustum_t& occlSet)
{
	if(!r_drawHeightfields.GetBool())
		return;

	bool renderTranslucency = (nDrawFlags & RFLAG_TRANSLUCENCY) > 0;

	if(renderTranslucency && !m_hasTransparentSubsets)
		return;

#ifdef EDITOR
	if(m_isChanged)
	{
		g_pShaderAPI->Reset(STATE_RESET_VBO);
		g_pShaderAPI->ApplyBuffers();

		// regenerate again
		GenereateRenderData();

		m_isChanged = false;
	}
#endif // EDITOR

	for(int i = 0; i < m_numBatches; i++)
	{
		hfielddrawbatch_t& batch = m_batches[i];

		if(!occlSet.IsBoxVisible(batch.bbox))
			continue;

		bool isTransparent = (batch.pMaterial->GetFlags() & MATERIAL_FLAG_TRANSPARENT) > 0;

		if(isTransparent != renderTranslucency)
			continue;

		materials->SetMatrix(MATRIXMODE_WORLD, identity4());
		materials->SetCullMode((nDrawFlags & RFLAG_FLIP_VIEWPORT_X) ? CULL_FRONT : CULL_BACK);

		g_pShaderAPI->SetVertexFormat(m_format);
		g_pShaderAPI->SetVertexBuffer(m_vertexbuffer, 0);
		g_pShaderAPI->SetIndexBuffer(m_indexbuffer);

		materials->BindMaterial(batch.pMaterial);

		g_pShaderAPI->DrawIndexedPrimitives(PRIM_TRIANGLES, batch.startIndex, batch.numIndices, batch.startVertex, batch.numVerts);
	}
}


void CHeightTileFieldRenderable::RenderDebug(ITexture* debugTexture, int nDrawFlags, const occludingFrustum_t& occlSet)
{
	bool renderTranslucency = (nDrawFlags & RFLAG_TRANSLUCENCY) > 0;

	for(int i = 0; i < m_numBatches; i++)
	{
		hfielddrawbatch_t& batch = m_batches[i];

		if(!occlSet.IsBoxVisible(batch.bbox))
			continue;

		bool isTransparent = (batch.pMaterial->GetFlags() & MATERIAL_FLAG_TRANSPARENT) > 0;

		if(isTransparent != renderTranslucency)
			continue;

		materials->SetMatrix(MATRIXMODE_WORLD, identity4());
		materials->SetCullMode((nDrawFlags & RFLAG_FLIP_VIEWPORT_X) ? CULL_FRONT : CULL_BACK);

		g_pShaderAPI->SetVertexFormat(m_format);
		g_pShaderAPI->SetVertexBuffer(m_vertexbuffer, 0);
		g_pShaderAPI->SetIndexBuffer(m_indexbuffer);

		materials->BindMaterial(batch.pMaterial, 0);

		g_pShaderAPI->SetTexture(debugTexture,0,0);

		materials->Apply();

		g_pShaderAPI->DrawIndexedPrimitives(PRIM_TRIANGLES, batch.startIndex, batch.numIndices, batch.startVertex, batch.numVerts);
	}
}