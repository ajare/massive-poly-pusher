#include "mpp/Config.h"

#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#include <Windows.h>
#endif

#include <cassert>
#include <numeric>
#include <regex>
#include <list>
#include <glew/glew.h>
#include <gl/gl.h>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/type_ptr.hpp>
#pragma warning(pop)

#include "utils/StringUtils.h"

#include "mpp/RenderSystem.h"
#include "mpp/Program.h"
#include "mpp/ProgramStream.h"

using namespace std;

namespace mpp
{
	/*
	 * Constructor.
	 *
	 */
	Program::Program(string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream)
		: Resource(name, "Program", renderSystem, resourceMgr, resourceStream)
		, mVertexShaderId(0)
		, mFragmentShaderId(0)
		, mSortId(0)
	{
	}

	/*
	 * Comparison operator.
	 *
	 */
	bool Program::operator==(Program const& other)
	{
		return this->getId() == other.getId();
	}

	/*
	 * Compile shader source.
	 *
	 */
	void Program::compileShader(uint32* id, string const& source, string const& sourceType)
	{
		char const* sourcePtr = source.c_str();
		glShaderSource(*id, 1, (const GLchar**)&sourcePtr, nullptr);
		glCompileShader(*id);

		// Check for errors
		GLint status;
		glGetShaderiv(*id, GL_COMPILE_STATUS, &status);

		if (status == GL_FALSE) 
		{
			string msg = "Could not compile " + sourceType + " shader.";
			
			GLint infoLogLength;
			glGetShaderiv(*id, GL_INFO_LOG_LENGTH, &infoLogLength);
			char* strInfoLog = new char[infoLogLength + 1];
			
			glGetShaderInfoLog(*id, infoLogLength, NULL, strInfoLog);
			msg += strInfoLog;
			
			delete[] strInfoLog;
			glDeleteShader(*id); 
			*id = 0;

			getRenderSystem()->logMessage(msg);
			getRenderSystem()->logMessage(source);
			throw exception(msg.c_str());
		}

		getRenderSystem()->logMessage("Compiled " + sourceType + " shader.");
	}
		
	/*
	 * Create the data required for the program from the resource stream.
	 *
	 */
	void Program::createImpl()
	{
		ProgramStream* pStr = dynamic_cast<ProgramStream*>(getResourceStream().get());
		if (!pStr)
		{
			throw exception("Program::createImpl() could not cast to type 'ProgramStream'.");
		}

		mVertexSource = pStr->getVertexSource();
		mFragmentSource = pStr->getFragmentSource();
	}

	/*
	 * Destroy the program data.
	 *
	 */
	void Program::destroyImpl()
	{
		mVertexSource.clear();
		mFragmentSource.clear();
	}

	/*
	 * Create a variable info instance.
	 *
	 */
	Program::VariableInfo Program::getVariableInfo(string const& def, string const& name, string const& type, ShaderType shaderType)
	{
		VariableInfo vi;
		vi.def = def;
		vi.name = name;
		vi.type = type;

		if (vi.type == "bool" || vi.type == "int" || vi.type == "uint" || vi.type == "float" || vi.type == "double" ||
			vi.type == "bvec2" || vi.type == "bvec3" || vi.type == "bvec4" ||
			vi.type == "ivec2" || vi.type == "ivec3" || vi.type == "ivec4" ||
			vi.type == "uvec2" || vi.type == "uvec3" || vi.type == "uvec4" ||
			vi.type == "vec2" || vi.type == "vec3" || vi.type == "vec4" ||
			vi.type == "dvec2" || vi.type == "dvec3" || vi.type == "dvec4")
		{
			if (vi.type == "bool" || vi.type == "int" || vi.type == "uint" || vi.type == "float" || vi.type == "double")
			{
				vi.numComponents = 1;
			}
			else if (vi.type == "bvec2" || vi.type == "ivec2" || vi.type == "uvec2" || vi.type == "vec2" || vi.type == "dvec2")
			{
				vi.numComponents = 2;
			}
			else if (vi.type == "bvec3" || vi.type == "ivec3" || vi.type == "uvec3" || vi.type == "vec3" || vi.type == "dvec3")
			{
				vi.numComponents = 3;
			}
			else if (vi.type == "bvec4" || vi.type == "ivec4" || vi.type == "uvec4" || vi.type == "vec4" || vi.type == "dvec4")
			{
				vi.numComponents = 4;
			}
		}

		if (shaderType == ShaderType::Vertex)
		{
			if (def == "in" || def == "passthrough")
			{
				if (mVertexAttributes.empty())
				{
					vi.streamOffset = 0;
				}
				else
				{
					vi.streamOffset = mVertexAttributes.back().streamOffset + vi.numComponents;
				}

				mVertexAttributes.push_back(vi);
			}
		}

		return vi;
	}

	/*
	 * Remove comments from GLSL source code.
	 *
	 */
	string Program::stripComments(std::string const& src)
	{
		string stripped = src;

		for (size_t i = 0; i < stripped.length(); ++i)
		{
			if (i < (stripped.length() - 1))
			{
				if (stripped[i] == '/')
				{
					if (stripped[i + 1] == '/')
					{
						// Find '\n'
						int commentStart = i, commentEnd = -1;
						for (size_t j = (size_t)commentStart; j < stripped.length(); ++j)
						{
							if (stripped[j] == '\n')
							{
								commentEnd = (int)j;
								break;
							}
						}

						// Check if we hit the end of the file
						if (commentEnd == -1)
						{
							commentEnd = stripped.length() - 1;
						}

						stripped.erase(commentStart, commentEnd - commentStart + 1);
						--i;
					}
					if (stripped[i + 1] == '*')
					{
						// Find '*/'
						int commentStart = i, commentEnd = -1;
						for (size_t j = (size_t)commentStart; j < stripped.length(); ++j)
						{
							if (stripped[j - 1] == '*' && stripped[j] == '/')
							{
								commentEnd = (int)j;
								break;
							}
						}

						// Check if we hit the end of the file
						if (commentEnd == -1)
						{
							throw exception("COuld not parse multi-line comment.");
						}

						stripped.erase(commentStart, commentEnd - commentStart + 1);
						--i;
					}
				}
			}
		}

		return stripped;
	}

	/*
	 * Split into lines by statement and newline
	 *
	 */
	vector<string> Program::splitSourceIntoLines(string const& src)
	{
		vector<string> lines;

		uint32 i = 0, j = 0;
		while (i != src.length())
		{
			char ch = src[i];
			if (ch == ';' || ch == '\n')
			{
				string line = src.substr(j, i - j);
				if (!line.empty())
				{
					lines.push_back(line);
				}

				lines.push_back(string(&ch, 1));
				j = i + 1;
			}

			i++;
		}

		string line = src.substr(j, i - j);
		if (!line.empty())
		{
			lines.push_back(line);
		}

		return lines;
	}

	/*
	 * Parse source file for special tokens.
	 *
	 */
	string Program::parseSource(string const& src, ShaderType shaderType, bool usingGeometryShader)
	{
		// Get caps and set variable standards
		auto rs = getRenderSystem();
		auto const& caps = rs->getCaps();

		string inDecl, outDecl;
		bool useDeprecated = caps.glslVersionMajor < 3;
		string inPrefix, outPrefix;
		switch (shaderType)
		{
		case ShaderType::Vertex:
			inDecl = useDeprecated ? "attribute" : "in";
			outDecl = useDeprecated ? "varying" : "out";
			inPrefix = MPP_PROGRAM_VS_IN_PREFIX;
			outPrefix = MPP_PROGRAM_VS_OUT_PREFIX;
			break;

		case ShaderType::Geometry:
			inDecl = useDeprecated ? "varying" : "in";
			outDecl = useDeprecated ? "varying" : "out";
			inPrefix = MPP_PROGRAM_VS_OUT_PREFIX;
			outPrefix = MPP_PROGRAM_GS_OUT_PREFIX;
			break;

		case ShaderType::Fragment:
			inDecl = useDeprecated ? "varying" : "in";
			outDecl = "out";
			inPrefix = usingGeometryShader ? MPP_PROGRAM_GS_OUT_PREFIX : MPP_PROGRAM_VS_OUT_PREFIX;
			outPrefix = MPP_PROGRAM_FS_OUT_PREFIX;
			break;
		}

		// Variable information
		list<VariableInfo> inVars, outVars, uniformVars, textureVars;
		bool mcpUsed = false, normalUsed = false, halfWindowSizeUsed = false;

		// Find entry point
		int mainLine = -1;
		regex entryPointRegex("void\\smain");
		smatch stringMatch;
		if (regex_search(src, stringMatch, entryPointRegex))
		{
			if (stringMatch.size() > 1)
			{
				throw exception("Multiple main definitions found in program source.");
			}

			mainLine = 0;
			int stringPos = stringMatch.position(0);
			while (stringPos >= 0)
			{
				if (src[stringPos] == '\n' || src[stringPos] == ';')
				{
					mainLine++;
				}
				
				stringPos--;
			}
		}
		else
		{
			throw exception("Could not find main() definition in program source.");
		}

		// See which built-in uniforms are used
		string strippedSrc = stripComments(src);

		// Check for special uniforms
		if (strippedSrc.find(MPP_PROGRAM_MCPMATRIX_TOKEN) != string::npos)
		{
			mcpUsed = true;
		}

		if (strippedSrc.find(MPP_PROGRAM_NORMALMATRIX_TOKEN) != string::npos)
		{
			normalUsed = true;
		}

		if (strippedSrc.find(MPP_PROGRAM_HALFWINDOWSIZE_TOKEN) != string::npos)
		{
			halfWindowSizeUsed = true;
		}

		// Parse source line by line (or by statement).
		vector<string> lines = splitSourceIntoLines(strippedSrc);

		list<string> parsedLines;
		bool prevLineWasTemplate = false;
		for (uint32 i = 0; i < lines.size(); ++i)
		{
			string line = lines[i];

			// If it's semicolon or '\n', ignore
			if ((line == "\n" && !prevLineWasTemplate) || line == ";")
			{
				parsedLines.push_back(line);
				continue;
			}

			// Parse lines[i], then replace it and append lines[i + 1] to it
			string trimmedLine = line; utils::StringUtils::trim(trimmedLine);

			prevLineWasTemplate = false;
			int templateSectionIndex = trimmedLine.find("@@");
			if (templateSectionIndex != string::npos && trimmedLine != "@@Version")
			{
				// Don't keep the next newline
				prevLineWasTemplate = true;
				mainLine--;

				string templateSection = trimmedLine.substr(templateSectionIndex + 2);
				utils::StringUtils::trim(templateSection);

				int spacePos = templateSection.find_first_of(' ');
				if (spacePos == string::npos)
				{
					string errMsg = "Invalid template definition '" + templateSection + "' in program source.";
					throw exception(errMsg.c_str());
				}

				// Variable definition
				string varDef = templateSection.substr(0, spacePos);
				utils::StringUtils::trim(varDef);

				// Variable name
				int equalsPos = templateSection.find_first_of('=');
				if (equalsPos == string::npos)
				{
					string errMsg = "Invalid template definition '" + templateSection + "' in program source.";
					throw exception(errMsg.c_str());
				}

				string varName = templateSection.substr(spacePos + 1, equalsPos - (spacePos + 1));
				utils::StringUtils::trim(varName);

				// Variable type
				string varType = templateSection.substr(equalsPos + 1);
				utils::StringUtils::trim(varType);

				transform(varDef.begin(), varDef.end(), varDef.begin(), ::tolower);

				VariableInfo vi = getVariableInfo(varDef, varName, varType, shaderType);

				if (varDef == "in")
				{
					inVars.push_back(vi);
				}
				else if (varDef == "out")
				{
					outVars.push_back(vi);
				}
				else if (varDef == "passthrough")
				{
					inVars.push_back(vi);
					outVars.push_back(vi);
				}
				else if (varDef == "uniform")
				{
					uniformVars.push_back(vi);
				}
				else if (varDef == "texture")
				{
					textureVars.push_back(vi);
				}
			}
			else
			{
				parsedLines.push_back(line);
			}
		}

		auto it = parsedLines.begin();
		for (int i = 0; i < mainLine; ++i, ++it);

		// If mcp or normal matrices, or half window size vector are used, insert them
		mMcpMatrixId = -1;
		mNormalMatrixId = -1;
		mHalfWindowSizeId = -1;
		if (mcpUsed)
		{
			parsedLines.insert(it, utils::StringUtils::format("uniform mat4 {};\n", MPP_PROGRAM_MCPMATRIX_NAME));
		}
		if (normalUsed)
		{
			parsedLines.insert(it, utils::StringUtils::format("uniform mat3 {};\n", MPP_PROGRAM_NORMALMATRIX_NAME));
		}
		if (halfWindowSizeUsed)
		{
			parsedLines.insert(it, utils::StringUtils::format("uniform vec2 {};\n", MPP_PROGRAM_HALFWINDOWSIZE_NAME));
		}

		parsedLines.insert(it, "\n");

		// Insert uniform definitions
		for (auto vit = uniformVars.begin(); vit != uniformVars.end(); ++vit)
		{
			auto const& vi = *vit;

			string markedUpUniform = MPP_PROGRAM_MARKUP_UNIFORM(vi.name);
			parsedLines.insert(it, utils::StringUtils::format("uniform {} {};\n", vi.type, markedUpUniform));

			mUniformIds[markedUpUniform] = -1;
		}

		parsedLines.insert(it, "\n");

		// Insert texture definitions
		for (auto vit = textureVars.begin(); vit != textureVars.end(); ++vit)
		{
			auto const& vi = *vit;

			string markedUpTexture = MPP_PROGRAM_MARKUP_TEXTURE(vi.name);
			parsedLines.insert(it, utils::StringUtils::format("uniform {} {};\n", vi.type, markedUpTexture));

			TextureInfo ti;
			ti.samplerName = vi.name;
			ti.markedUpName = markedUpTexture;
			ti.uniformId = -1;

			mTextures.push_back(ti);
		}

		parsedLines.insert(it, "\n");
		
		// Insert in-var definitions
		int layoutLocation = 0;
		for (auto vit = inVars.begin(); vit != inVars.end(); ++vit)
		{
			auto const& vi = *vit;

			parsedLines.insert(it, utils::StringUtils::format("{} {} {}{}_;\n", inDecl, vi.type, inPrefix, vi.name));
			layoutLocation++;
		}

		parsedLines.insert(it, "\n");

		// Insert out-var definitions
		for (auto vit = outVars.begin(); vit != outVars.end(); ++vit)
		{
			auto const& vi = *vit;

			parsedLines.insert(it, utils::StringUtils::format("{} {} {}{}_;\n", outDecl, vi.type, outPrefix, vi.name));
		}

		parsedLines.insert(it, "\n");

		// Token replace @() instances in code.
		string parsedSource = accumulate(parsedLines.begin(), parsedLines.end(), string(""));

		utils::StringUtils::replaceAll(parsedSource, MPP_PROGRAM_MCPMATRIX_TOKEN, MPP_PROGRAM_MCPMATRIX_NAME);
		utils::StringUtils::replaceAll(parsedSource, MPP_PROGRAM_NORMALMATRIX_TOKEN, MPP_PROGRAM_NORMALMATRIX_NAME);
		utils::StringUtils::replaceAll(parsedSource, MPP_PROGRAM_HALFWINDOWSIZE_TOKEN, MPP_PROGRAM_HALFWINDOWSIZE_NAME);

		// Add version
		string versionString = utils::StringUtils::toString(caps.glslVersionMajor) + utils::StringUtils::toString(caps.glslVersionMinor);
		utils::StringUtils::replaceAll(parsedSource, "@@Version", "#version " + versionString + "\n\n");
		utils::StringUtils::replaceAll(parsedSource, "@Version", versionString);

		vector<string> tokenList;
		tokenList.push_back("@In(");
		tokenList.push_back("@Out(");
		tokenList.push_back("@Uniform(");
		tokenList.push_back("@Texture(");
		
		for (auto const& token: tokenList)
		{
			int templateStart = parsedSource.find(token);
			while (templateStart != string::npos)
			{
				// Find closing bracket
				int templateEnd = parsedSource.find(")", templateStart + token.length());

				if (templateEnd == string::npos)
				{
					throw exception("Could not find closing template bracket in program source.");
				}

				string tokenName = parsedSource.substr(templateStart + token.length(), templateEnd - templateStart - token.length());
				utils::StringUtils::trim(tokenName);

				string markedUpTokenName;

				// Mark up token
				if (token == "@In(")
				{
					markedUpTokenName = inPrefix + tokenName + "_";

					// Check that it has been declared
					if (find_if(inVars.begin(), inVars.end(), [tokenName](VariableInfo const& vi)
					{
						return vi.name == tokenName;
					}) == inVars.end())
					{
						string errMsg = "In-variable '" + tokenName + "' used but not declared.";
						throw exception(errMsg.c_str());
					}
				}
				if (token == "@Out(")
				{
					markedUpTokenName = outPrefix + tokenName + "_";

					// Check that it has been declared
					if (find_if(outVars.begin(), outVars.end(), [tokenName](VariableInfo const& vi)
					{
						return vi.name == tokenName;
					}) == outVars.end())
					{
						string errMsg = "Out-variable '" + tokenName + "' used but not declared.";
						throw exception(errMsg.c_str());
					}
				}
				if (token == "@Uniform(")
				{
					markedUpTokenName = MPP_PROGRAM_MARKUP_UNIFORM(tokenName);

					// Check that it has been declared
					if (find_if(uniformVars.begin(), uniformVars.end(), [tokenName](VariableInfo const& vi)
					{
						return vi.name == tokenName;
					}) == uniformVars.end())
					{
						string errMsg = "Uniform '" + tokenName + "' used but not declared.";
						throw exception(errMsg.c_str());
					}
				}
				if (token == "@Texture(")
				{
					markedUpTokenName = MPP_PROGRAM_MARKUP_TEXTURE(tokenName);

					// Check that it has been declared
					if (find_if(textureVars.begin(), textureVars.end(), [tokenName](VariableInfo const& vi)
					{
						return vi.name == tokenName;
					}) == textureVars.end())
					{
						string errMsg = "Uniform (texture) '" + tokenName + "' used but not declared.";
						throw exception(errMsg.c_str());
					}
				}

				string tokenText = parsedSource.substr(templateStart, templateEnd - templateStart + 1);
				utils::StringUtils::replaceAll(parsedSource, tokenText, markedUpTokenName);
				templateStart = parsedSource.find(token);
			}
		}

		// Insert in/out code: find final } in main()
		// Go through each line after main (including after main text in the main line)
		// and and find first {.  Then find matching }, and put this on a new line.  Then
		// insert inout code before it.
		if (shaderType != ShaderType::Fragment)
		{
			int cursorPos = parsedSource.find("void main(");
			cursorPos = parsedSource.find("{", cursorPos) + 1;
			int bracketStack = 1;

			while (cursorPos < (int)parsedSource.length())
			{
				if (parsedSource[cursorPos] == '{')
				{
					bracketStack++;
				}
				else if (parsedSource[cursorPos] == '}')
				{
					bracketStack--;
				}

				if (bracketStack == 0)
				{
					// Found the end of main.
					string passthroughs = "\n\n";
					for (auto const& inout : inVars)
					{
						if (inout.def == "passthrough")
						{
							passthroughs += utils::StringUtils::format("\t{}{}_ = {}{}_;\n", outPrefix, inout.name, inPrefix, inout.name);
						}
					}

					parsedSource.insert(cursorPos - 1, passthroughs);
					break;
				}

				cursorPos++;
			}
		}

		return parsedSource;
	}

	/*
	 * Create OpenGL program.
	 *
	 */
	void Program::loadImpl()
	{
		auto rs = getRenderSystem();
		try
		{
			// Create vertex shader
			mVertexSource = parseSource(mVertexSource, ShaderType::Vertex, false);
			mFragmentSource = parseSource(mFragmentSource, ShaderType::Fragment, false);

			mVertexShaderId = glCreateShader(GL_VERTEX_SHADER);
			if (mVertexShaderId == 0)
			{
				throw exception("Could not create vertex shader id.");
			}

			compileShader(&mVertexShaderId, mVertexSource, "vertex");

			// Create fragment shader
			mFragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
			if (mFragmentShaderId == 0)
			{
				throw exception("Could not create fragment shader id.");
			}

			compileShader(&mFragmentShaderId, mFragmentSource, "fragment");

			// Create program
			GLuint programId = glCreateProgram();
			if (programId == 0)
			{
				throw exception("Could not create program id.");
			}

			// Attach shaders
			glAttachShader(programId, mVertexShaderId);
			glAttachShader(programId, mFragmentShaderId);

			// Bind attributes
			for (uint32 i = 0; i < mVertexAttributes.size(); ++i)
			{
				auto const& vertexAttrib = mVertexAttributes[i];

				string markedUpName = MPP_PROGRAM_VS_IN_PREFIX + vertexAttrib.name + "_";
				glBindAttribLocation(programId, i, markedUpName.c_str());
			}

			// Link shaders
			glLinkProgram(programId);

			// Detach and destroy them to free memory
			glDetachShader(programId, mVertexShaderId);
			glDeleteShader(mVertexShaderId);
			mVertexShaderId = 0;

			glDetachShader(programId, mFragmentShaderId);
			glDeleteShader(mFragmentShaderId);
			mFragmentShaderId = 0;

			GLint status;
			glGetProgramiv(programId, GL_LINK_STATUS, &status);

			if (status == GL_FALSE)
			{
				string msg = "Could not link program: ";

				GLint infoLogLength;
				glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &infoLogLength);
				char* strInfoLog = new char[infoLogLength + 1];

				glGetProgramInfoLog(programId, infoLogLength, NULL, strInfoLog);
				msg += strInfoLog;

				delete[] strInfoLog;
				glDeleteProgram(programId);
				programId = 0;

				rs->logMessage(msg);
				throw exception(msg.c_str());
			}

			setId(programId);

			// Get attribute information
			int attribCount;
			glGetProgramiv(programId, GL_ACTIVE_ATTRIBUTES, &attribCount);
			for (int i = 0; i < attribCount; ++i)
			{
				//GLint location = glGetAttribLocation(programId, mVertexAttributes[i].name.c_str());

				const GLsizei bufSize = 256;
				GLsizei lengthWritten;
				GLint varSize;
				GLenum varType;
				char varNameBuffer[bufSize];

				glGetActiveAttrib(programId, i, bufSize - 1, &lengthWritten, &varSize, &varType, varNameBuffer);
				varNameBuffer[lengthWritten] = 0;

				int location = glGetAttribLocation(programId, varNameBuffer);
				string varName = varNameBuffer;

				string typeName;
				switch (varType)
				{
				case GL_FLOAT_VEC2: typeName = "vec2"; break;
				case GL_FLOAT_VEC3: typeName = "vec3"; break;
				case GL_FLOAT_VEC4: typeName = "vec4"; break;
				case GL_FLOAT_MAT2: typeName = "mat2"; break;
				case GL_FLOAT_MAT3: typeName = "mat3"; break;
				case GL_FLOAT_MAT4: typeName = "mat4"; break;
				default: typeName = "unknown type"; break;
				}
				
				rs->logMessage(utils::StringUtils::format("Vertex attribute {}: {} ({}[{}]).", location, varName, typeName, varSize));
			}

			// Get uniform information
			int uniformCount;
			glGetProgramiv(programId, GL_ACTIVE_UNIFORMS, &uniformCount);

			for (int i = 0; i < uniformCount; ++i)
			{
				const GLsizei bufSize = 256;
				GLsizei lengthWritten;
				GLint uniformSize;
				GLenum uniformType;
				char uniformNameBuffer[bufSize];

				glGetActiveUniform(programId, i, bufSize - 1, &lengthWritten, &uniformSize, &uniformType, uniformNameBuffer);
				uniformNameBuffer[lengthWritten] = 0;

				string uniformName = uniformNameBuffer;

				// Is this MCPMatrix, NormalMatrix, standard uniform or a texture?
				if (uniformName == MPP_PROGRAM_MCPMATRIX_NAME)
				{
					mMcpMatrixId = glGetUniformLocation(programId, uniformNameBuffer);
					rs->logMessage("ModelCameraProjection matrix id: " + utils::StringUtils::toString(mMcpMatrixId));
				}
				else if (uniformName == MPP_PROGRAM_NORMALMATRIX_NAME)
				{
					mNormalMatrixId = glGetUniformLocation(programId, uniformNameBuffer);
					rs->logMessage("Normal matrix id: " + utils::StringUtils::toString(mNormalMatrixId));
				}
				else if (uniformName == MPP_PROGRAM_HALFWINDOWSIZE_NAME)
				{
					mHalfWindowSizeId = glGetUniformLocation(getId(), uniformNameBuffer);
					rs->logMessage("Half window size id: " + utils::StringUtils::toString(mHalfWindowSizeId));
				}
				else if (mUniformIds.find(uniformName) != mUniformIds.end())
				{
					mUniformIds[uniformName] = glGetUniformLocation(programId, uniformNameBuffer);
					rs->logMessage("Uniform '" + uniformName + "' id: " + utils::StringUtils::toString(mUniformIds[uniformName]));
				}
				else
				{
					auto it = find_if(mTextures.begin(), mTextures.end(), [uniformName](TextureInfo const& ti) -> bool
					{
						return ti.markedUpName == uniformName;
					});

					if (it != mTextures.end())
					{
						it->uniformId = glGetUniformLocation(programId, uniformNameBuffer);
						rs->logMessage("Texture (uniform) '" + uniformName + "' id: " + utils::StringUtils::toString(it->uniformId));
					}
					else
					{
						// Uniform used but not declared in metadata
						string errMsg = "Uniform '" + uniformName + "' was used in program '" + getName() + "' but not declared in metadata.";
						throw exception(errMsg.c_str());
					}
				}
			}
		}
		catch (exception& e)
		{
			rs->logMessage("Error creating program '" + getName() + "'.");
			throw e;
		}

		rs->logMessage("Created program '" + getName() + "'.");
		rs->logMessage("");
	}

	/*
	 * Destroy the OpenGL program.
	 *
	 */
	void Program::unloadImpl()
	{
		GLuint id = getId();
		if (id != 0)
		{
			if (mVertexShaderId != 0)
			{
				glDeleteShader(mVertexShaderId);
				mVertexShaderId = 0;
			}

			if (mFragmentShaderId != 0)
			{
				glDeleteShader(mFragmentShaderId);
				mFragmentShaderId = 0;
			}

			glDeleteProgram(id);
			setId(0);
		}
	}

	/*
	 * Get attribute index.
	 *
	 */
	int32 Program::getAttrib(char const* name) const
	{
		GLint attrib = glGetAttribLocation(getId(), name);
		/*assert(attrib >= 0 && "Program::getAttrib() attribute not found!");*/
		return attrib;
	}
	
	/*
	 * Get index for specified uniform.
	 *
	 */
	int Program::getUniformId(string const& name) const
	{
		string markedUpUniform = MPP_PROGRAM_MARKUP_UNIFORM(name);
		if (mUniformIds.find(markedUpUniform) == mUniformIds.end())
		{
			return -1;
		}
		else
		{
			return mUniformIds.at(markedUpUniform);
		}
	}

	//	string markedUpTexture = MPP_PROGRAM_MARKUP_TEXTURE(texture);

	/*
	 * Get index for model-camera-projection matrix, if using one, or -1.
	 *
	 */
	int Program::getModelCameraProjectionMatrixId() const
	{
		return mMcpMatrixId;
	}

	/*
	 * Get index for normal matrix, if using one, or -1.
	 *
	 */	
	int Program::getNormalMatrixId() const
	{
		return mNormalMatrixId;
	}

	/*
	 * Get index for half window size uniform, if using it, or -1.
	 *
	 */	
	int Program::getHalfWindowSizeId() const
	{
		return mHalfWindowSizeId;
	}

	/*
	 * Have a separate id for sorting to the GL id, as the GL id may not fit in however many bytes
	 * we have assigned to program in the sort key.
	 *
	 */
	void Program::setSortId(uint32 sortId)
	{
		mSortId = sortId;
	}

	/*
	 * Get the sort id.
	 *
	 */
	uint32 Program::getSortId() const
	{
		return mSortId;
	}

	/*
	 * Get sampler count.
	 *
	 */
	int Program::getNumSamplers() const
	{
		return mTextures.size();
	}

	/*
	 * Get sampler name.
	 *
	 */
	string const& Program::getSamplerName(int index) const
	{
		return mTextures[index].samplerName;
	}

	/*
	 * Activate program
	 *
	 */
	void Program::bind()
	{
		glUseProgram(getId());

		// Bind texture unit locations to samplers
		for (uint32 i = 0; i < mTextures.size(); ++i)
		{
			auto const& ti = mTextures[i];
			glUniform1i(ti.uniformId, i);
		}
	}
}

