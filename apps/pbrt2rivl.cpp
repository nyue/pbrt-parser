// ======================================================================== //
// Copyright 2016-2017 Ingo Wald                                            //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

// pbrt
#include "pbrt/Parser.h"
// stl
#include <iostream>
#include <vector>
#include <sstream>

namespace pbrt_parser {

  using std::cout;
  using std::endl;

  /*! the scene we're exporting. currently a global so material
    exporter can look up globally named materials., sho;ld probably
    move this to a 'exporter' class at some time... */
  std::shared_ptr<Scene> scene;
  
  FileName basePath = "";

  // the output file we're writing.
  FILE *out = NULL;
  FILE *bin = NULL;

  size_t numUniqueTriangles = 0;
  size_t numInstancedTriangles = 0;
  size_t numUniqueObjects = 0;
  size_t numInstances = 0;
  size_t numMaterials = 0;

  std::map<std::shared_ptr<Shape>,int> alreadyExported;
  //! transform used when original instance was emitted
  std::map<int,affine3f> transformOfFirstInstance;
  std::map<int,int> numTrisOfInstance;

  size_t nextTransformID = 0;

  std::vector<int> rootObjects;

  inline std::string prettyNumber(const size_t s) {
    double val = s;
    char result[100];
    if (val >= 1e12f) {
      sprintf(result,"%.1fT",val/1e12f);
    } else if (val >= 1e9f) {
      sprintf(result,"%.1fG",val/1e9f);
    } else if (val >= 1e6f) {
      sprintf(result,"%.1fM",val/1e6f);
    } else if (val >= 1e3f) {
      sprintf(result,"%.1fK",val/1e3f);
    } else {
      sprintf(result,"%lu",s);
    }
    return result;
  }


  int nextNodeID = 0;

  /*! export a given material, and return its rivl node ID */
  int exportMaterial(std::shared_ptr<Material> material);
  
  /*! export a param we _know_ if a vec3f */
  std::string exportMaterialParam3f(const std::string &name, std::shared_ptr<Param> _param)
  {
    const ParamT<float> *param = dynamic_cast<ParamT<float>*>(_param.get());
    if (!param)
      throw std::runtime_error("could not type-cast "+_param->toString()+" to param of floats!?");
    const std::vector<float> &v = param->paramVec;
    if (v.size() != 3)
      throw std::runtime_error("parameter "+_param->toString()+" does not have exactly 3 data element(s)!?");

    std::stringstream ss;
    ss << " <param name=\"" << name << "\" type=\"float3\">" << v[0] << " " << v[1] << " " << v[2] << "</param>" << std::endl;
    return ss.str();
  }

  /*! export a param we _know_ if a float */
  std::string exportMaterialParam1f(const std::string &name, std::shared_ptr<Param> _param)
  {
    const ParamT<float> *param = dynamic_cast<ParamT<float>*>(_param.get());
    if (!param)
      throw std::runtime_error("could not type-cast "+_param->toString()+" to param of floats!?");
    const std::vector<float> &v = param->paramVec;
    if (v.size() != 1)
      throw std::runtime_error("parameter "+_param->toString()+" does not have exactly 1 data element(s)!?");

    std::stringstream ss;
    ss << " <param name=\"" << name << "\" type=\"float\">" << v[0] << "</param>" << std::endl;
    return ss.str();
  }
  
  /*! export a param we _know_ if a float */
  std::string exportMaterialParamTexture(const std::string &name, std::shared_ptr<Param> _param)
  {
    const ParamT<std::string> *param = dynamic_cast<ParamT<std::string>*>(_param.get());
    if (!param)
      throw std::runtime_error("could not type-cast "+_param->toString()+" to param of std::string!?");
    const std::vector<std::string> &v = param->paramVec;
    if (v.size() != 1)
      throw std::runtime_error("parameter "+_param->toString()+" does not have exactly 1 data element(s)!?");

    const std::string textureName = v[0];
    std::shared_ptr<Texture> texture = scene->namedTexture[textureName];
    if (!texture)
      throw std::runtime_error("could not find named texture '"+textureName+"'");

    PRINT(texture->toString());
    const std::string fileName = texture->getParamString("filename");
    PRINT(fileName);
    std::stringstream ss;
    std::cout << "#pbrt: currently ignoring textures ... :-( " << std::endl;
    // ss << " <param name=\"" << name << "\" type=\"float\">" << v[0] << "</param>" << std::endl;
    return ss.str();
  }
  
  /*! export a param we _know_ if a std::string */
  std::string exportMaterialParamString(const std::string &name, std::shared_ptr<Param> _param)
  {
    const ParamT<std::string> *param = dynamic_cast<ParamT<std::string>*>(_param.get());
    if (!param)
      throw std::runtime_error("could not type-cast "+_param->toString()+" to param of std::string's!?");
    const std::vector<std::string> &v = param->paramVec;
    if (v.size() != 1)
      throw std::runtime_error("parameter "+_param->toString()+" does not have exactly 1 data element(s)!?");

    std::stringstream ss;
#if 1
    std::cout << "#pbrt: warning - rivl can't handle strings yet ... :-(" << std::endl;
    std::cout << "#pbrt:  in: " << name << " = " << _param->toString() << std::endl;
#else
    ss << " <param name=\"" << name << "\" type=\"string\">" << v[0] << "</param>";
#endif
    return ss.str();
  }
  
  /*! export a param we _know_ if a std::string */
  std::string exportMaterialParamMaterial(const std::string &name, std::shared_ptr<Param> _param)
  {
    const ParamT<std::string> *param = dynamic_cast<ParamT<std::string>*>(_param.get());
    if (!param)
      throw std::runtime_error("could not type-cast "+_param->toString()+" to param of std::string's!?");
    const std::vector<std::string> &v = param->paramVec;
    if (v.size() != 1)
      throw std::runtime_error("parameter "+_param->toString()+" does not have exactly 1 data element(s)!?");

    std::stringstream ss;

    std::string childMatName = v[0];
    std::shared_ptr<Material> childMat = scene->namedMaterial[childMatName];
    int childMatID = exportMaterial(childMat);
    if (!childMat)
      throw std::runtime_error("trying to export material named '"+childMatName+"' that doesn't seem to exist!?");
    ss << " <param name=\"" << name << "\" type=\"material\">" << childMatID << "</param>";
    
    return ss.str();
  }
  
  
  /*! export a general material parameter, whicever it might be. if
      this includes additional references like textures, other
      mateirals, etc, those will get recursively exported first */
  std::string exportMaterialParam(/*! the material we're in - for debugging outsputs only */
                                  std::shared_ptr<Material> material,
                                  /*! name of the parameter */
                                  const std::string &name,
                                  /*! data values of the parameter */
                                  std::shared_ptr<Param> param)
  {
    /* ugh - this is really ugly in pbrt - mateirals have type
       'string', so we have to detect that here based on hardcoded
       material and parameter name(s) */
    if (material->type == "mix" && name == "namedmaterial1") 
      return exportMaterialParamMaterial(name,param);
    if (material->type == "mix" && name == "namedmaterial2") 
      return exportMaterialParamMaterial(name,param);

    
    const std::string type = param->getType();
    if (type == "color" || type == "rgb")
      return exportMaterialParam3f(name,param);
    else if (type == "texture")
      return exportMaterialParamTexture(name,param);
    else if (type == "float")
      return exportMaterialParam1f(name,param);
    else if (type == "float")
      return exportMaterialParamTexture(name,param);
    else if (type == "string")
      return exportMaterialParamString(name,param);
    else {
      std::cout << "#pbrt: WARNING - don't know what to do with material param "
                << material->type << "::" << name << " = " << param->toString() << std::endl;
      return "";
    }
  }
  
  /*! export a given material, and return its rivl node ID */
  int exportMaterial(std::shared_ptr<Material> material)
  {
    if (!material) 
      // default material
      return 0;
    
    // std::cout << "-------------------------------------------------------" << std::endl;
    // std::cout << "exporting material " << material->type << std::endl;
    // std::cout << "-------------------------------------------------------" << std::endl;
    
    static std::map<std::shared_ptr<Material>,int> alreadyExported;
    if (alreadyExported.find(material) != alreadyExported.end())
      return alreadyExported[material];
    
    std::stringstream ss;
    for (auto paramPair : material->param) {
      const std::string name = paramPair.first;
      auto param = paramPair.second;
      ss << exportMaterialParam(material,name,param);
    }
    

    int thisID = nextNodeID++;
    alreadyExported[material] = thisID;

    fprintf(out,"<Material name=\"ignore\" type=\"%s\" id=\"%i\">\n",material->type.c_str(),thisID);
    // ss << "<Material name=\"doesntMatter\" type=\""<< material->type << "\">" << endl;
    fprintf(out,"%s",ss.str().c_str());
    // ss << "</Material>" << std::endl;
    fprintf(out,"</Material>\n");
    
    numMaterials++;
    return thisID;
  }

  int writeTriangleMesh(std::shared_ptr<Shape> shape, const affine3f &instanceXfm)
  {
    numUniqueObjects++;
    std::shared_ptr<Material> mat = shape->material;
    // cout << "writing shape " << shape->toString() << " w/ material " << (mat?mat->toString():"<null>") << endl;

    int materialID = exportMaterial(shape->material);

    int thisID = nextNodeID++;
    const affine3f xfm = instanceXfm*shape->transform;
    alreadyExported[shape] = thisID;
    transformOfFirstInstance[thisID] = xfm;

    fprintf(out,"<Mesh id=\"%i\">\n",thisID);
    fprintf(out,"  <materiallist>%i</materiallist>\n",materialID);
    { // parse "point P"
      std::shared_ptr<ParamT<float> > param_P = shape->findParam<float>("P");
      if (param_P) {
        size_t ofs = ftell(bin);
        const size_t numPoints = param_P->paramVec.size() / 3;
        for (int i=0;i<numPoints;i++) {
          vec3f v(param_P->paramVec[3*i+0],
                  param_P->paramVec[3*i+1],
                  param_P->paramVec[3*i+2]);
          v = xfmPoint(xfm,v);
          fwrite(&v,sizeof(v),1,bin);
          // fprintf(out,"v %f %f %f\n",v.x,v.y,v.z);
          // numVerticesWritten++;
        }
        fprintf(out,"  <vertex num=\"%li\" ofs=\"%li\"/>\n",
                numPoints,ofs);
      }
        
    }
      
    { // parse "int indices"
      std::shared_ptr<ParamT<int> > param_indices = shape->findParam<int>("indices");
      if (param_indices) {
        size_t ofs = ftell(bin);
        const size_t numIndices = param_indices->paramVec.size() / 3;
        numTrisOfInstance[thisID] = numIndices;
        numUniqueTriangles+=numIndices;
        for (int i=0;i<numIndices;i++) {
          vec4i v(param_indices->paramVec[3*i+0],
                  param_indices->paramVec[3*i+1],
                  param_indices->paramVec[3*i+2],
                  0);
          fwrite(&v,sizeof(v),1,bin);
        }
        fprintf(out,"  <prim num=\"%li\" ofs=\"%li\"/>\n",
                numIndices,ofs);
      }
    }        
    fprintf(out,"</Mesh>\n");
    return thisID;
  }

  void parsePLY(const std::string &fileName,
                std::vector<vec3f> &v,
                std::vector<vec3f> &n,
                std::vector<vec3i> &idx);

  int writePlyMesh(std::shared_ptr<Shape> shape, const affine3f &instanceXfm)
  {
    std::vector<vec3f> p, n;
    std::vector<vec3i> idx;
      
    numUniqueObjects++;
    std::shared_ptr<Material> mat = shape->material;
    int materialID = exportMaterial(shape->material);

    std::shared_ptr<ParamT<std::string> > param_fileName = shape->findParam<std::string>("filename");
    FileName fn = FileName(basePath) + param_fileName->paramVec[0];
    parsePLY(fn.str(),p,n,idx);

    int thisID = nextNodeID++;
    const affine3f xfm = instanceXfm*shape->transform;
    alreadyExported[shape] = thisID;
    transformOfFirstInstance[thisID] = xfm;
      
    // -------------------------------------------------------
    fprintf(out,"<Mesh id=\"%i\">\n",thisID);
    fprintf(out,"  <materiallist>%i</materiallist>\n",materialID);

    // -------------------------------------------------------
    fprintf(out,"  <vertex num=\"%li\" ofs=\"%li\"/>\n",
            p.size(),ftell(bin));
    for (int i=0;i<p.size();i++) {
      vec3f v = xfmPoint(xfm,p[i]);
      fwrite(&v,sizeof(v),1,bin);
    }

    // -------------------------------------------------------
    fprintf(out,"  <prim num=\"%li\" ofs=\"%li\"/>\n",
            idx.size(),ftell(bin));
    for (int i=0;i<idx.size();i++) {
      vec3i v = idx[i];
      fwrite(&v,sizeof(v),1,bin);
      int z = 0.f;
      fwrite(&z,sizeof(z),1,bin);
    }
    numTrisOfInstance[thisID] = idx.size();
    numUniqueTriangles += idx.size();
    // -------------------------------------------------------
    fprintf(out,"</Mesh>\n");
    // -------------------------------------------------------
    return thisID;
  }

  void writeObject(const std::shared_ptr<Object> &object, 
                   const affine3f &instanceXfm)
  {
    // cout << "writing " << object->toString() << endl;

    // std::vector<int> child;

    for (int shapeID=0;shapeID<object->shapes.size();shapeID++) {
      std::shared_ptr<Shape> shape = object->shapes[shapeID];

      numInstances++;

      if (alreadyExported.find(shape) != alreadyExported.end()) {
          

        int childID = alreadyExported[shape];
        affine3f xfm = instanceXfm * //shape->transform * 
          rcp(transformOfFirstInstance[childID]);
        numInstancedTriangles += numTrisOfInstance[childID];

        int thisID = nextNodeID++;
        fprintf(out,"<Transform id=\"%i\" child=\"%i\">\n",
                thisID,
                childID);
        fprintf(out,"  %f %f %f\n",
                xfm.l.vx.x,
                xfm.l.vx.y,
                xfm.l.vx.z);
        fprintf(out,"  %f %f %f\n",
                xfm.l.vy.x,
                xfm.l.vy.y,
                xfm.l.vy.z);
        fprintf(out,"  %f %f %f\n",
                xfm.l.vz.x,
                xfm.l.vz.y,
                xfm.l.vz.z);
        fprintf(out,"  %f %f %f\n",
                xfm.p.x,
                xfm.p.y,
                xfm.p.z);
        fprintf(out,"</Transform>\n");
        rootObjects.push_back(thisID);
        continue;
      } 
      
      if (shape->type == "trianglemesh") {
        int thisID = writeTriangleMesh(shape,instanceXfm);
        rootObjects.push_back(thisID);
        continue;
      }

      if (shape->type == "plymesh") {
        int thisID = writePlyMesh(shape,instanceXfm);
        rootObjects.push_back(thisID);
        continue;
      }

      cout << "**** invalid shape #" << shapeID << " : " << shape->type << endl;
    }
    for (int instID=0;instID<object->objectInstances.size();instID++) {
      writeObject(object->objectInstances[instID]->object,
                  instanceXfm*object->objectInstances[instID]->xfm);
    }      
  }

  void usage(const std::string &errMsg = "")
  {
    if (!errMsg.empty())
      std::cout << "Error: " << errMsg << std::endl << std::endl;
    
    std::cout << "Usage:  ./pbrt2rivl inFileName.pbrt -o outFileName.xml [args]" << std::endl;
    std::cout << "w/ args: " << std::endl;
    std::cout << "  -o <filename.xml>       specify out-filename" << std::endl;
    std::cout << "  -h|--help               print this help message" << std::endl;
    std::cout << "  -path <input-path>      path to scene file, if required (usually it isn't)" << std::endl;

    exit(errMsg != "");
  }

  void pbrt2rivl(int ac, char **av)
  {
    std::vector<std::string> fileName;
    bool dbg = false;
    std::string outFileName = "";
    for (int i=1;i<ac;i++) {
      const std::string arg = av[i];
      if (arg[0] == '-') {
        if (arg == "-dbg" || arg == "--dbg")
          dbg = true;
        else if (arg == "--path" || arg == "-path")
          basePath = av[++i];
        else if (arg == "-h"|| arg == "--help")
          usage();
        else if (arg == "-o")
          outFileName = av[++i];
        else
          THROW_RUNTIME_ERROR("invalid argument '"+arg+"'");
      } else {
        fileName.push_back(arg);
      }          
    }
    if (fileName.empty()) usage("no pbrt input file name(s) specified");
    if (outFileName.empty()) usage("no output file name specified (-o)");

    out = fopen(outFileName.c_str(),"w");
    bin = fopen((outFileName+".bin").c_str(),"w");
    assert(out);
    assert(bin);

    fprintf(out,"<?xml version=\"1.0\"?>\n");
    fprintf(out,"<BGFscene>\n");

    int thisID = nextNodeID++;
    fprintf(out,"<Material name=\"default\" type=\"OBJMaterial\" id=\"%i\">\n",thisID);
    fprintf(out,"  <param name=\"kd\" type=\"float3\">0.7 0.7 0.7</param>\n");
    fprintf(out,"</Material>\n");
  
    std::cout << "-------------------------------------------------------" << std::endl;
    std::cout << "parsing:";
    for (int i=0;i<fileName.size();i++)
      std::cout << " " << fileName[i];
    std::cout << std::endl;

    if (basePath.str() == "")
      basePath = FileName(fileName[0]).path();
  
    std::shared_ptr<pbrt_parser::Parser> parser = std::make_shared<pbrt_parser::Parser>(dbg,basePath);
    try {
      for (int i=0;i<fileName.size();i++)
        parser->parse(fileName[i]);
    
      std::cout << "==> parsing successful (grammar only for now)" << std::endl;
    
      scene = parser->getScene();
      writeObject(scene->world,ospcommon::one);
      scene = nullptr;
      {
        int thisID = nextNodeID++;
        fprintf(out,"<Group id=\"%i\" numChildren=\"%lu\">\n",thisID,rootObjects.size());
        for (int i=0;i<rootObjects.size();i++)
          fprintf(out,"%i ",rootObjects[i]);
        fprintf(out,"\n</Group>\n");
      }

      fprintf(out,"</BGFscene>");

      fclose(out);
      fclose(bin);
      cout << "Done exporting to OSP file" << endl;
      cout << " - unique objects/shapes    " << prettyNumber(numUniqueObjects) << endl;
      cout << " - num instances (inc.1sts) " << prettyNumber(numInstances) << endl;
      cout << " - unique triangles written " << prettyNumber(numUniqueTriangles) << endl;
      cout << " - instanced tris written   " << prettyNumber(numUniqueTriangles+numInstancedTriangles) << endl;
      cout << " - num materials            " << prettyNumber(numMaterials) << endl;
    } catch (std::runtime_error e) {
      std::cout << "**** ERROR IN PARSING ****" << std::endl << e.what() << std::endl;
      exit(1);
    }
  }

} // ::pbrt_parser

int main(int ac, char **av)
{
  pbrt_parser::pbrt2rivl(ac,av);
  return 0;
}
