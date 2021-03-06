///////////////////////////////////////////////////////////////////////////////
///
/// \file RawDocumentation.cpp
/// Slight reimplementations of the documentation classes for Raw doc
///
/// Authors: Joshua Shlemmer
/// Copyright 2015-2016, DigiPen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "Precompiled.hpp"

#include "RawDocumentation.hpp"
#include "Serialization/Simple.hpp"
#include "Platform\FileSystem.hpp"
#include "TinyXmlHelpers.hpp"
#include "MacroDatabase.hpp"

#include <Engine/Documentation.hpp>

namespace Zero
{

  bool LoadCommandList(CommandDocList& commandList, StringParam absPath)
  {
    Status status;
    DataTreeLoader loader;

    if (!loader.OpenFile(status, absPath))
    {
      Error("Unable to load command list file: %s\n", absPath.c_str());
      return false;
    }

    PolymorphicNode dummyNode;
    loader.GetPolymorphic(dummyNode);

    loader.SerializeField("Commands", commandList.mCommands);

    loader.Close();

    printf("...successfully loaded command list from file...\n");
    return true;
  }

  bool NameIsSwizzleOperation(StringParam methodName)
  {

    String name = methodName.ToUpper();
    
    if (name.Contains("GET"))
      name = name.SubString(name.FindFirstOf("GET").End(), name.End());
    else if (name.Contains("SET"))
      name = name.SubString(name.FindFirstOf("SET").End(), name.End());

    if (name.Empty())
      return false;

    // first do the trivial check if the fn is named too long to be swizzle
    if (name.ComputeRuneCount() > 4)
      return false;

    const String validCharacters = "XYZW";

    forRange(Rune rune, name.All())
    {
      if (!validCharacters.Contains(String(rune)))
      {
        return false;
      }
    }
    return true;
  }

  bool LoadEventList(EventDocList& eventList, StringParam absPath)
  {
    Status status;
    DataTreeLoader loader;

    if (!loader.OpenFile(status, absPath))
    {
      Error("Unable to load command list file: %s\n", absPath.c_str());
      return false;
    }

    PolymorphicNode docNode;
    loader.GetPolymorphic(docNode);

    PolymorphicNode eventListObject;
    loader.GetPolymorphic(eventListObject);

    loader.SerializeField("Events", eventList.mEvents);

    loader.Close();

    printf("...successfully loaded event list from file...\n");
    return true;
  }

  bool SaveTrimDocToDataFile(DocumentationLibrary &lib, StringParam absPath)
  {
    Status status;

    TextSaver saver;
    saver.Open(status, absPath.c_str());

    if (status.Failed())
    {
      Error(status.Message.c_str());
      return false;
    }

    saver.StartPolymorphic("DocumentationLibrary");

    saver.SerializeField("Classes", lib.mClasses);

    saver.SerializeField("Enums", lib.mEnums);

    saver.SerializeField("Flags", lib.mFlags);

    saver.EndPolymorphic();

    saver.Close();
    return true;
  }

  ////////////////////////////////////////////////////////////////////////
  // Helpers
  ////////////////////////////////////////////////////////////////////////
  bool LoadDocumentationSkeleton(DocumentationLibrary &skeleton, StringParam file)
  {
    Status status;
    DataTreeLoader loader;

    if (!loader.OpenFile(status, file))
    {
      Error("Unable to load documentation skeleton file: %s\n", file.c_str());
      return false;
    }
     
    PolymorphicNode docLibraryNode;
    loader.GetPolymorphic(docLibraryNode);

    loader.SerializeField("Classes", skeleton.mClasses);
    loader.SerializeField("Enums", skeleton.mEnums);
    loader.SerializeField("Flags", skeleton.mFlags);
    
    loader.Close();

    printf("...successfully loaded doc skeleton from file...\n");
    return true;
  }

  String GetArgumentIfString(TypeTokens &fnCall, uint argPos)
  {
    uint start = (uint)-1;
    // get first OpenParen index
    for (uint i = 0; i < fnCall.Size(); ++i)
    {
      if (fnCall[i].mEnumTokenType == DocTokenType::OpenParen)
      {
        start = i;
        break;
      }
    }

    // nope out of here if we did not find a starting paren
    if (start == (uint)-1)
      return "";

    int parenCount = 0;
    uint argCount = 0;
    // start iterating from the character after that to the end
    for (uint i = start; i < fnCall.Size(); ++i)
    {
      DocToken &currToken = fnCall[i];

      // if we hit an OpenParen
      if (currToken.mEnumTokenType == DocTokenType::OpenParen)
      {
        // add it to our open paren counter and continue
        ++parenCount;
        continue;
      }
      // if we hit a CloseParen
      else if (currToken.mEnumTokenType == DocTokenType::CloseParen)
      {
        // subtract from our paren counter and continue
        --parenCount;
        continue;
      }
      // if parenCount is greater then one since we count the first one
      //just skip this token by continuing
      if (parenCount > 1)
        continue;

      // if token is a comma
      if (currToken.mEnumTokenType == DocTokenType::Comma)
      {
        // add it to our arg counter
        ++argCount;
        // if arg counter is equal to argPos
        if (argCount == argPos)
        {
          // check if prev token is a string
          // if it is, return it
          // otherwise return an empty string
          if (fnCall[i - 1].mEnumTokenType == DocTokenType::StringLiteral)
            return fnCall[i - 1].mText;
          return "";
        }
      }
    }
    return "";
  }

  String GetCodeFromDocumentFile(TiXmlDocument *doc)
  {
    StringBuilder codeBuilder;

    // grab the class
    TiXmlElement* cppDef = doc->FirstChildElement(gElementTags[eDOXYGEN])
      ->FirstChildElement(gElementTags[eCOMPOUNDDEF]);

    TiXmlElement *programList = GetFirstNodeOfChildType(cppDef, "programlisting")->ToElement();

    TiXmlNode *firstCodeline = GetFirstNodeOfChildType(programList, gElementTags[eCODELINE]);
    TiXmlNode *endCodeline = GetEndNodeOfChildType(programList, gElementTags[eCODELINE]);

    for (TiXmlNode *codeline = firstCodeline;
      codeline != endCodeline;
      codeline = codeline->NextSibling())
    {
      StringBuilder lineBuilder;

      GetTextFromAllChildrenNodesRecursively(codeline, &lineBuilder);

      if (lineBuilder.GetSize() == 0)
        continue;

      String codeString = lineBuilder.ToString();

      if (codeString.c_str()[0] == '#' ||
        (codeString.SizeInBytes() > 1 && codeString.c_str()[0] == '/' && codeString.c_str()[1] == '/'))
      {
        continue;
      }

      codeBuilder << codeString;
    }

    return codeBuilder.ToString();
  }

  String GetDoxyfileNameFromSourceFileName(StringParam filename)
  {
    StringBuilder builder;

    builder << '_';

    StringRange extensionLocation = filename.FindLastOf('.');

    // get spot after dot
    extensionLocation.IncrementByRune();

    String extension(extensionLocation.Begin(), filename.End());

    if (extension != "cpp" && extension != "hpp")
      return "";

    bool prevLowercase = false;

    // the minus four should give us every character before extension
    for(uint i = 0; i < filename.SizeInBytes() - 4; ++i)
    {
      char c = filename.c_str()[i];

      if (IsUpper(c) && prevLowercase)
        builder << '_';

      prevLowercase = IsLower(c);

      builder << (char)ToLower(c);
    }

    builder << "_8" << extension;

    builder << ".xml";

    return builder.ToString();
  }

  String TrimTypeTokens(const TypeTokens &tokens)
  {
    StringBuilder builder;

    TypeTokens newList;

    // We don't care about the last token since it is not going to be a scope resolution
    for (uint i = 0; i < tokens.Size(); ++i)
    {
      const DocToken &currToken = tokens[i];
      // If I don't find a token "::", skip this token we don't care about it
      if (currToken.mEnumTokenType != DocTokenType::ScopeResolution)
      {
        newList.Append(currToken);
      }
      // Check to the right of the scope resolution token for an enum.
      // If there is an enum token
      else if (tokens[i + 1].mText == "Enum")
      {
        // Skip "::" and skip the enum token
        ++i;
      }
      // If no enum
      else
      {
        // Skip "::" and remove token to the left of it
        newList.PopBack();
      }
    }

    for (uint i = 0; i < newList.Size(); ++i)
    {
      const DocToken &token = newList[i];

      builder.Append(token.mText);

      if (token.mEnumTokenType > DocTokenType::QualifiersStart)
        builder.Append(" ");
    }
    return builder.ToString();
  }

  DocDfaState* DocLangDfa::Get(void)
  {
    static DocDfaState *instance;

    return instance ? instance : instance = CreateLangDfa();
  }

  // returns first child of element with value containing tag type 'type'
  TiXmlNode* GetFirstNodeOfChildType(TiXmlElement* element, StringParam type)
  {
    for (TiXmlNode* node = element->FirstChild(); node; node = node->NextSibling())
    {
      if (type == node->Value())
        return node;
    }
    return nullptr;
  }

  // returns one past the last chiled of tag type 'type', returns null if it DNE
  TiXmlNode* GetEndNodeOfChildType(TiXmlElement* element, StringParam type)
  {
    TiXmlNode* prevNode = nullptr;
    for (TiXmlNode* node = element->LastChild(); node; node = node->PreviousSibling())
    {
      if (type == node->Value())
        return prevNode;
      prevNode = node;
    }
    return nullptr;
  }

  // does as it says, removes all spaces from str and outputs it into the stringbuilder
  void CullAllSpacesFromString(StringParam str, StringBuilder* output)
  {
    forRange(Rune c, str.All())
    {
      if (c != ' ')
        output->Append(c);
    }
  }

  // just keep trodding our way down the node structure until we get every single text node
  // (I am tired of edge cases making us miss text so this is the brute force hammer)
  void GetTextFromAllChildrenNodesRecursively(TiXmlNode* node, StringBuilder* output)
  {
    for (TiXmlNode *child = node->FirstChild(); child != nullptr; child = child->NextSibling())
    {
      if (child->Type() != TiXmlNode::TEXT)
      {
        GetTextFromAllChildrenNodesRecursively(child, output);
        continue;
      }

      output->Append(child->Value());
      output->Append(" ");
    }
  }

  // call other overload but return string instead of getting passed a string builder
  String GetTextFromAllChildrenNodesRecursively(TiXmlNode* node)
  {
    StringBuilder output;

    GetTextFromAllChildrenNodesRecursively(node, &output);

    return output.ToString();
  }

  void GetTextFromChildrenNodes(TiXmlNode* node, StringBuilder* output)
  {
    TiXmlElement* element = node->ToElement();

    TiXmlNode* firstNode = GetFirstNodeOfChildType(element, gElementTags[eTYPE]);

    if (firstNode)
    {
      DebugBreak();
    }

    TiXmlNode* endNode = GetEndNodeOfChildType(element, gElementTags[eTYPE]);

    if (!firstNode)
    {
      TiXmlNode* firstNode = GetFirstNodeOfChildType(element, gElementTags[eHIGHLIGHT]);

      if (firstNode != nullptr)
        DebugBreak();

      TiXmlNode* endNode = GetEndNodeOfChildType(element, gElementTags[eHIGHLIGHT]);
    }

    if (firstNode == nullptr)
    {
      node = element->FirstChild();
      endNode = element->LastChild();

      for (;node != endNode->NextSibling(); node = node->NextSibling())
      {
        if (node->Type() == TiXmlNode::TEXT)
        {
          output->Append(node->Value());
          output->Append(" ");
        }
      }
    }
    else for (TiXmlNode* node = firstNode; node != endNode; node = node->NextSibling())
    {
      output->Append(node->Value());
      output->Append(" ");
    }
  }

  void getTextFromParaNodes(TiXmlNode* node, StringBuilder* output)
  {
    TiXmlElement* element = node->ToElement();

    TiXmlNode* firstPNode = GetFirstNodeOfChildType(element, gElementTags[ePARA]);
    TiXmlNode* endPNode = GetEndNodeOfChildType(element, gElementTags[ePARA]);

    for (TiXmlNode* paraNode = firstPNode; paraNode != endPNode; paraNode = paraNode->NextSibling())
    {
      for (TiXmlNode* node = paraNode->FirstChild(); node != nullptr; node = node->NextSibling())
      {
        if (node->Type() == TiXmlNode::TEXT)
        {
          output->Append(node->ToText()->Value());
          output->Append(" ");
        }
        else if (node->Type() == TiXmlNode::ELEMENT)
        {
          for (TiXmlNode* eleChild = node->FirstChild();
            eleChild != nullptr;
            eleChild = eleChild->NextSibling())
          {
            output->Append(GetTextFromAllChildrenNodesRecursively(eleChild));
            output->Append(" ");
          }
        }
      }
    }
  }

  // replaces token at location with the tokens from the typedef passed in
  uint ReplaceTypedefAtLocation(TypeTokens& tokenArray
    , DocToken* location, RawTypedefDoc& tDef)
  {
    TypeTokens newArray;

    forRange(DocToken& token, tokenArray.All())
    {
      if (&token == location)
      {
        forRange(DocToken& defToken, tDef.mDefinition.All())
        {
          newArray.PushBack(defToken);
        }
      }
      else
      {
        newArray.PushBack(token);
      }
    }

    tokenArray = newArray;

    return tDef.mDefinition.Size() - 1;
  }

  // get type string from an element that contains Text nodes
  void BuildFullTypeString(TiXmlElement* element, StringBuilder* output)
  {
    TiXmlNode* typeNode = GetFirstNodeOfChildType(element, gElementTags[eTYPE]);

    if (!typeNode)
      return;

    GetTextFromAllChildrenNodesRecursively(typeNode, output);
  }


  // replaces type tokens with corresponding mTypedefs
  // returns true if any replacements were made
  bool NormalizeTokensFromTypedefs(TypeTokens& tokens, RawTypedefLibrary* defLib, 
    RawNamespaceDoc& classNamespace)
  {
    bool madeReplacements = false;

    // loop over tokens
    for (uint i = 0; i < tokens.Size(); ++i)
    {
      //should be moved into the loop
      Array<String>& names = classNamespace.mNames;

      DocToken& token = tokens[i];

      // A lot of types have "Param" in them, just trim it from the type
      StringRange paramRange = token.mText.FindLastOf("Param");
      if (!paramRange.Empty())
      {
        token.mText = token.mText.SubString(token.mText.Begin(), paramRange.Begin());
      }

      // check any namespaces this token list is inside
      
      String key = token.mText;

      // loop over possible namespace'd version
      for (int j = -1; j < (int)names.Size(); ++j)
      {
        StringBuilder builder;

        for (uint k = 0; (int)k <= j; ++k)
          builder.Append(names[k]);

        builder.Append(token.mText);
        
        key = builder.ToString();

        if (defLib->mTypedefs.ContainsKey(key))
        {
          TypeTokens* typedefTokens = &defLib->mTypedefs[key]->mDefinition;

          // make sure they are not just the same tokens
          if (tokens.Size() >= typedefTokens->Size() 
            && ContainsFirstTypeInSecondType(*typedefTokens, tokens))//tokens == *typedefTokens)
          {
            break;
          }
          // this next chunk of crazy to make sure we don't redundantly expand any typedefs
          else
          {
            // first we have to make sure our token ranges are of valid size
            if ((int)i - 2 > 0 && typedefTokens->Size() >= 3)
            {
              bool equal = true;

              // the magic number 3 is because we are checking for this: typedef name ns::name
              for (uint m = 0; m < 3; ++m)
              { 
                if ((*typedefTokens)[m].mText != tokens.SubRange(i - 2, 3)[m].mText)
                  equal = false;
              }
              if (equal)
              {
                break;
              }
            }
          }

          madeReplacements = true;
          i += ReplaceTypedefAtLocation(tokens, &token,*defLib->mTypedefs[key]);
          break;
        }
      }
      
    }
    return madeReplacements;
  }

  void GetFilesWithPartialName(StringParam basePath,StringParam partialName, Array<String>* output)
  {
    FileRange range(basePath);
    for (; !range.Empty(); range.PopFront())
    {
      FileEntry entry = range.frontEntry();

      // if this is a subdirectory, recurse down the directory
      String filePath = entry.GetFullPath();
      if (IsDirectory(filePath))
      {
        GetFilesWithPartialName(filePath, partialName, output);
      }
      // if we are not a directory, see if we are a file with partialName contained in filename
      else
      {
        if (entry.mFileName.Contains(partialName))
          output->PushBack(filePath);
      }
    }
  }

  void GetFilesWithPartialName(StringParam basePath, StringParam partialName,
    IgnoreList &ignoreList, Array<String>* output)
  {
    FileRange range(basePath);
    for (; !range.Empty(); range.PopFront())
    {
      FileEntry entry = range.frontEntry();

      // if this is a subdirectory, recurse down the directory
      String filePath = entry.GetFullPath();

      if (ignoreList.DirectoryIsOnIgnoreList(filePath))
      {
        WriteLog("Ignoring file/directory: %s\n", filePath.c_str());
        return;
      }

      if (IsDirectory(filePath))
      {
        GetFilesWithPartialName(filePath, partialName, ignoreList, output);
      }
      // if we are not a directory, see if we are a file with partialName contained in filename
      else
      {
        if (entry.mFileName.Contains(partialName))
          output->PushBack(filePath);
      }
    }
  }

  String GetFileWithExactName(StringParam basePath, StringParam exactName)
  {
    FileRange range(basePath);
    for (; !range.Empty(); range.PopFront())
    {
      FileEntry entry = range.frontEntry();

      // if this is a subdirectory, recurse down the directory
      String filePath = entry.GetFullPath();
      if (IsDirectory(filePath))
      {
        String retval = GetFileWithExactName(filePath, exactName);
        if (retval == "")
          continue;
        return retval;
      }
      // if we are not a directory, see if we are a file with partialName contained in filename
      else
      {
        if (entry.mFileName == exactName)
          return filePath;
      }
    }
    return "";
  }

  String CleanRedundantSpacesInDesc(StringParam description)
  {
    StringBuilder builder;

    bool prevSpace = false;

    for (uint i = 0; i < description.SizeInBytes(); ++i)
    {
      char currChar = description.c_str()[i];

      if (currChar == ' ')
      {
        if (prevSpace)
          continue;

        prevSpace = true;
      }
      else
        prevSpace = false;

      builder.Append(currChar);
    }
    return builder.ToString();
  }

  void OutputListOfObjectsWithoutDesc(const DocumentationLibrary &trimDoc,
    IgnoreList *ignoreList)
  {
    DocLogger *log = DocLogger::Get();

    WriteLog("Objects Missing Description:\n");

    for (uint i = 0; i < trimDoc.mClasses.Size(); ++i)
    {
      ClassDoc *currClass = trimDoc.mClasses[i];

      String &className = currClass->mName;

      // if we are already supposed to ignore this, we don't care it is undocumented
      if (ignoreList && ignoreList->NameIsOnIgnoreList(className))
      {
        continue;
      }


      if (currClass->mDescription == "")
        WriteLog("Class '%s' missing description\n", currClass->mName.c_str());

      for (uint j = 0; j < currClass->mMethods.Size(); ++j)
      {
        MethodDoc *currMethod = currClass->mMethods[j];
        if (currMethod->mDescription == "")
        {
          String& methodName = currMethod->mName;

          // if on the ignore list
          if (NameIsSwizzleOperation(methodName) || (ignoreList && ignoreList->NameIsOnIgnoreList(methodName)))
          {
            continue;
          }

          WriteLog("Method '%s.%s' missing description\n"
            , currClass->mName.c_str(), currMethod->mName.c_str());
        }

      }
      for (uint j = 0; j < currClass->mProperties.Size(); ++j)
      {
        PropertyDoc *currProp = currClass->mProperties[j];
        if (currProp->mDescription == "")
        {
          String& propName = currProp->mName;

          // if on the ignore list
          if (NameIsSwizzleOperation(propName) || (ignoreList && ignoreList->NameIsOnIgnoreList(propName)))
          {
            continue;
          }

          WriteLog("Property '%s.%s' missing description\n"
            , currClass->mName.c_str(), currProp->mName.c_str());
        }
      }
    }
  }

  bool ContainsFirstTypeInSecondType(TypeTokens &firstType, TypeTokens &secondType)
  {
    if (firstType.Empty())
      return false;

    bool containedType = false;
    uint matchStart = (uint)-1;

    // we have to find the start of the match
    for (uint i = 0; i < secondType.Size(); ++i)
    {
      if (secondType[i] == firstType[0])
      {
        containedType = true;
        matchStart = i;
        break;
      }
    }
    if (!containedType)
      return false;

    uint matchEnd = matchStart;
    for (uint i = 0; i < firstType.Size(); ++i)
    {
      if (matchStart + i > secondType.Size()
        || !(firstType[i] == secondType[i + matchStart]))
      {
        return false;
      }
    }
    return true;
  }

  String TrimNamespacesOffOfName(StringParam name)
  {
    // if it is an enum, take the namespace name
    if (name.Contains("Enum"))
    {
      return name.SubString(name.Begin(), name.FindFirstOf(':').Begin());
    }
    // otherwise, we are going to trim off all namespaces
    else
    {
      return name.SubString(name.FindLastOf(':').End(), name.End());
    }
  }

  // compares two document classes by alphabetical comparison of names
  template<typename T>
  bool DocCompareFn(const T& lhs, const T& rhs)
  {
    return lhs.Name < rhs.Name;
  }

  // compares two document classes by alphebetical comparison of names (for pointer types)
  template<typename T>
  bool DocComparePtrFn(const T& lhs, const T& rhs)
  {
    return lhs->mName < rhs->mName;
  }

  // specific version for typedef because it is a special snowflake
  bool TypedefDocCompareFn(const RawTypedefDoc& lhs, const RawTypedefDoc& rhs)
  {
    return lhs.mType < rhs.mType;
  }

  ////////////////////////////////////////////////////////////////////////
  // IgnoreList
  ////////////////////////////////////////////////////////////////////////
  //ZeroDefineType(IgnoreList);
  ZilchDefineType(IgnoreList, builder, type)
  {

  }

  void IgnoreList::Serialize(Serializer& stream)
  {
    SerializeName(mDirectories);
    SerializeName(mIgnoredNames);
  }

  template<> struct Zero::Serialization::Trait<IgnoreList>
  {

    enum { Type = StructureType::Object };
    static inline cstr TypeName() { return "IgnoreList"; }
  };

  bool IgnoreList::DirectoryIsOnIgnoreList(StringParam dir) const
  {
    StringRange locationRange = dir.FindFirstOf(mDoxyPath);

    uint location = locationRange.SizeInBytes();

    String relativeDir;
    if (location != (uint)-1)
      relativeDir = dir.SubStringFromByteIndices(mDoxyPath.SizeInBytes(), dir.SizeInBytes());
    else
      relativeDir = dir;

    if (mDirectories.Contains(relativeDir))
      return true;

    // now recursivly check to see if we are in a directory that was ignored
    StringRange subPath = relativeDir.SubString(relativeDir.Begin(), relativeDir.FindLastOf('\\').Begin());

    while (subPath.SizeInBytes() > 2)
    {
      if (mDirectories.Contains(relativeDir))
        return true;

      subPath.PopBack();
      subPath = subPath.SubString(subPath.Begin(), subPath.FindLastOf('\\').Begin());
    }

    return false;
  }

  bool IgnoreList::NameIsOnIgnoreList(StringParam name) const
  {
    // strip all namespaces off of the name
    String strippedName;
    if (name.Contains(":"))
    {
      StringRange subStringStart = name.FindLastOf(':');
      subStringStart.IncrementByRune();
      strippedName = name.SubString(subStringStart.Begin(), name.End());

      return mIgnoredNames.Contains(strippedName);
    }

    return mIgnoredNames.Contains(name);
  }

  bool IgnoreList::empty(void)
  {
    return mDirectories.Empty() || mIgnoredNames.Empty();
  }

  void IgnoreList::CreateIgnoreListFromDocLib(StringParam doxyPath, DocumentationLibrary &doc)
  {
    // for each class inside of the library
    for (uint i = 0; i < doc.mClasses.Size(); ++i)
    {
      String& name = doc.mClasses[i]->mName;
      String doxName = GetDoxygenName(name);

      // add array of all of the files that pertain to that class to ignored directories
      //GetFilesWithPartialName(doxyPath, doxName, &mDirectories);

      // add the name to the ignored names list
      mIgnoredNames.Insert(name);
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // DocLogger
  ////////////////////////////////////////////////////////////////////////
  DocLogger::~DocLogger()
  {
    if (mStarted)
      mLog.Close();
  }
  void DocLogger::StartLogger(StringParam path, bool verbose)
  {
    this->mPath = path;

    mStarted = true;

    mVerbose = verbose;

    StringRange folderPath = mPath.SubString(path.Begin(), mPath.FindLastOf('\\').Begin());

    if (!DirectoryExists(folderPath))
    {
      CreateDirectoryAndParents(folderPath);
    }
  }

  void DocLogger::Write(const char*msgFormat...)
  {
    va_list args;
    va_start(args, msgFormat);
    //Get the number of characters needed for message
    int bufferSize;
    ZeroVSPrintfCount(msgFormat, args, 1, bufferSize);

    char* msgBuffer = (char*)alloca((bufferSize + 1) * sizeof(char));

    ZeroVSPrintf(msgBuffer, bufferSize + 1, msgFormat, args);

    va_end(args);

    printf(msgBuffer);

    if (mStarted)
    {
      if (!mLog.IsOpen())
      {
        ErrorIf(!mLog.Open(mPath, FileMode::Write, FileAccessPattern::Sequential),
          "failed to open log at: %s\n", mPath);
      }

      StringBuilder builder;

      if (mVerbose)
      {
        // we timestamp logs if verbose
        CalendarDateTime time = Time::GetLocalTime(Time::GetTime());

        // month first since we are heathens
        builder << '[' << time.Hour << ':' << time.Minutes
          << " (" << time.Month << '/' << time.Day
          << '/' << time.Year << ")] - ";
      }

      builder << (const char *)msgBuffer;

      String timestampMsg = builder.ToString();

      mLog.Write((byte*)timestampMsg.c_str(), timestampMsg.SizeInBytes());

      mLog.Close();
    }
  }

  DocLogger* DocLogger::Get(void)
  {
    static DocLogger logger;

    return &logger;
  }

  ////////////////////////////////////////////////////////////////////////
  // DocLogger
  ////////////////////////////////////////////////////////////////////////
  bool AttributeLoader::LoadUserAttributeFile(StringParam fileLocation)
  {
    Status status;
    DataTreeLoader loader;

    if (!loader.OpenFile(status, fileLocation))
    {
      Error("Unable to load user attributes file: %s\n", fileLocation.c_str());
      return false;
    }

    mAttributes = new AttributeDocList();

    //PolymorphicNode docNode;
    //loader.GetPolymorphic(docNode);

    PolymorphicNode attribListObject;
    loader.GetPolymorphic(attribListObject);

    loader.SerializeField("ObjectAttributes", mAttributes->mObjectAttributes);
    loader.SerializeField("FunctionAttributes", mAttributes->mFunctionAttributes);
    loader.SerializeField("PropertyAttributes", mAttributes->mPropertyAttributes);

    loader.Close();

    printf("...successfully loaded user attributes file from file...\n");
    return true;
  }

  bool AttributeLoader::loadArrayOfAttributes(Array<AttributeDoc*>&attribList, HashMap<String, AttributeDoc *>& attribMap, StringParam doxyFile)
  {
    TiXmlDocument doc;

    if (!doc.LoadFile(doxyFile.c_str()))
    {
      return false;
    }

    // grab the class
    TiXmlElement* namespaceDef = doc.FirstChildElement(gElementTags[eDOXYGEN])
      ->FirstChildElement(gElementTags[eCOMPOUNDDEF])->FirstChildElement(gElementTags[eSECTIONDEF]);

    // first child compound def is the actual namespace: ObjectAttributes, 

    // get first attribute
    TiXmlElement *attributeElement = namespaceDef->FirstChildElement(gElementTags[eMEMBERDEF]);

    // iterate over every attribute
    for(; attributeElement != nullptr; attributeElement = attributeElement->NextSiblingElement())
    {
      String name = attributeElement->FirstChildElement(gElementTags[eNAME])->GetText();

      name = name.SubString(name.FindFirstOf("c").End(), name.End());

      // get the description of the attribute
      String description = DoxyToString(attributeElement, gElementTags[eBRIEFDESCRIPTION]).Trim();

      if (attribMap.ContainsKey(name))
      {
        attribMap[name]->mDescription = description;
      }
      else
      {
        // create a new attrib doc. Note, no way to know static or multiple with developer stuff
        AttributeDoc* newAttrib = new AttributeDoc();

        newAttrib->mName = name;
        newAttrib->mDescription = description;
        newAttrib->mDeveloperAttribute = true;

        attribList.PushBack(newAttrib);
      }
    }
    return true;
  }

  bool AttributeLoader::LoadAttributeListsFromDoxygen(StringParam doxyPath)
  {
    FinalizeAttributeFile();
    // if loading the class file  failed, search for a struct file
    String fileName = FindFile(doxyPath, "namespace_zero_1_1_object_attributes.xml");

    if (!fileName.Empty())
    {
      if(!loadArrayOfAttributes(mAttributes->mObjectAttributes,
        mAttributes->mObjectAttributesMap, fileName))
      {
        WriteLog("Unable to load doxygen file to load object attributes.");
        return false;
      }
    }
    else
    {
      WriteLog("Unable to find doxygen file to load object attributes.");
      return false;
    }

    fileName = FindFile(doxyPath, "namespace_zero_1_1_function_attributes.xml");

    if (!fileName.Empty())
    {
      if(!loadArrayOfAttributes(mAttributes->mFunctionAttributes,
        mAttributes->mFunctionAttributesMap, fileName))
      {
        WriteLog("Unable to load doxygen file to load function attributes.");
        return false;
      }
    }
    else
    {
      WriteLog("Unable to find doxygen file to load function attributes.");
      return false;
    }

    fileName = FindFile(doxyPath, "namespace_zero_1_1_property_attributes.xml");

    if (!fileName.Empty())
    {
      if(!loadArrayOfAttributes(mAttributes->mPropertyAttributes,
        mAttributes->mPropertyAttributesMap, fileName))
      {
        WriteLog("Unable to load doxygen file to load property attributes.");
        return false;
      }
    }
    else
    {
      WriteLog("Unable to find doxygen file to load property attributes.");
      return false;
    }
    return true;
  }

  void AttributeLoader::FinalizeAttributeFile(void)
  {
    mAttributes->Sort();
    mAttributes->CreateAttributeMap();
  }

  bool AttributeLoader::SaveAttributeFile(StringParam outputFile)
  {
    return mAttributes->SaveToFile(outputFile);
  }

  AttributeDocList *AttributeLoader::GetAttributeList(void)
  {
    return mAttributes;
  }

  ////////////////////////////////////////////////////////////////////////
  // RawNamespaceDoc
  ////////////////////////////////////////////////////////////////////////
  void RawNamespaceDoc::Serialize(Serializer& stream)
  {
    SerializeName(mNames);
  }

  template<> struct Zero::Serialization::Trait<RawNamespaceDoc>
  {

    enum { Type = StructureType::Object };
    static inline cstr TypeName() { return "RawNamespaceDoc"; }
  };

  void RawNamespaceDoc::GetNamesFromTokens(TypeTokens& tokens)
  {
    for (uint i = 0; i < tokens.Size(); ++i)
    {
      DocToken& token = tokens[i];

      if (token.mEnumTokenType != DocTokenType::Identifier)
        continue;

      mNames.PushBack(token.mText);
    }
  }

  ////////////////////////////////////////////////////////////////////////
  // RawDocumentationLibrary
  ////////////////////////////////////////////////////////////////////////
  //ZeroDefineType(RawDocumentationLibrary);
  ZilchDefineType(RawDocumentationLibrary, builder, type)
  {

  }

  template<> struct Zero::Serialization::Trait<RawDocumentationLibrary>
  {

    enum { Type = StructureType::Object };
    static inline cstr TypeName() { return "RawDocumentationLibrary"; }
  };

  RawDocumentationLibrary::~RawDocumentationLibrary()
  {
    forRange(RawClassDoc* classDoc, mClasses.All())
    {
      delete classDoc;
    }
  }

  RawClassDoc* RawDocumentationLibrary::AddNewClass(StringParam className)
  {
    RawClassDoc* newClass = new RawClassDoc;
    mClasses.PushBack(newClass);

    newClass->mName = className;

    newClass->mParentLibrary = this;

    mClassMap[newClass->mName] = newClass;

    return newClass;
  }

  void RawDocumentationLibrary::FillTrimmedDocumentation(DocumentationLibrary &trimLib)
  {
    trimLib.mEnums = mEnums;
    trimLib.mFlags = mFlags;
    for (uint i = 0; i < mClasses.Size(); ++i)
    {
      ClassDoc* newClass = new ClassDoc();
      trimLib.mClasses.PushBack(newClass);
      mClasses[i]->FillTrimmedClass(newClass);

      trimLib.mClassMap[newClass->mName] = newClass;
    }
  }

  // create path if it does not exist
  // remove file of same name if it already exists/overwrite it
  // write string out to file at location
  // output error if we fail
  void RawDocumentationLibrary::GenerateCustomDocumentationFiles(StringParam directory)
  {
    WriteLog("writing raw documentation library to directory: %s\n\n", directory.c_str());
    forRange(RawClassDoc* classDoc, mClasses.All())
    {
      String absOutputPath = BuildString(directory, classDoc->mRelativePath);

      // if we have no classpath that means the class was never loaded to begin with
      if (classDoc->mRelativePath == "")
      {
        if (classDoc->mLibrary == "Core")
        {
          classDoc->mRelativePath = BuildString("\\BaseZilchTypes\\", classDoc->mName, ".data");
        }
        else
        {
          WriteLog("empty Class found by the name of: %s\n", classDoc->mName.c_str());
          continue;
        }
      }

      StringRange path = 
        absOutputPath.SubString(absOutputPath.Begin(), absOutputPath.FindLastOf('\\').Begin());

      if (!DirectoryExists(path))
      {
        CreateDirectoryAndParents(path);
      }

      // save class to file by the classes name, check return for fail print output
      if (!classDoc->SaveToFile(absOutputPath))
      {
        WriteLog("failed to write raw class data file at: %s\n", absOutputPath.c_str());
        Error("failed to write documentation to file at: %s\n", absOutputPath.c_str());
        continue;
      }

      mClassPaths.PushBack(classDoc->mRelativePath);
    }

    String docLibFile = BuildString(directory, "\\", "Library", ".data");
    //SaveToFile
    if (!SaveToFile(docLibFile))
    {
      WriteLog("failed to write library data file at: %s\n", docLibFile.c_str());
      Error("failed to write library data file at: %s\n", docLibFile.c_str());
      return;
    }

    printf("done writing raw documentation library\n");
  }

  void RawDocumentationLibrary::FillOverloadDescriptions(void)
  {
    // check each class and see if it has a function missing a mDescription
    forRange(RawClassDoc* classDoc, mClasses.All())
    {
      forRange(RawMethodDoc* methodDoc, classDoc->mMethods.All())
      {
        // if we have a description we do not need to do anything
        if (!methodDoc->mDescription.Empty())
          continue;

        // see if there is a function in that same class by the same name
        if (classDoc->mMethodMap[methodDoc->mName].Size() > 1)
        {
          // if there is, copy that mDescription into this method documentation
          methodDoc->mDescription = classDoc->GetDescriptionForMethod(methodDoc->mName);

          // if we found a mDescription, move on to the next method
          if (methodDoc->mDescription.SizeInBytes() > 0)
            continue;
        }

        // if not, check if the base class has a method by the same name
        RawClassDoc* parentClass = mClassMap.FindValue(classDoc->mBaseClass, nullptr);

        while (parentClass)
        {
          if (parentClass->mMethodMap[methodDoc->mName].Size() > 1)
          {
            methodDoc->mDescription = parentClass->GetDescriptionForMethod(methodDoc->mName);
          }

          // if we found a mDescription, move on to the next method
          if (methodDoc->mDescription.SizeInBytes() > 0)
            break;

          parentClass = mClassMap.FindValue(parentClass->mBaseClass, nullptr);
        }
      }
    }
  }

  void RawDocumentationLibrary::NormalizeAllTypes(RawTypedefLibrary* defLib)
  {
    forRange(RawClassDoc* classDoc, mClasses.All())
    {
      classDoc->NormalizeAllTypes(defLib);
    }
  }

  void RawDocumentationLibrary::LoadZilchTypeCppClassList(StringParam absPath)
  {
    Status status;
    DataTreeLoader loader;

    if (!loader.OpenFile(status, absPath))
    {
      Error("Unable to load ZilchTypeToCppClassList at: '%s'\n", absPath.c_str());
    }

    // get the base object
    PolymorphicNode dummyNode;
    loader.GetPolymorphic(dummyNode);
    
    // get the actual map
    loader.SerializeField("Map", mZilchTypeToCppClassList);

    loader.Close();

    printf("...successfully loaded ZilchTypeCppClassList from file...\n");
  }

  void RawDocumentationLibrary::Build(void)
  {
    forRange(RawClassDoc* classDoc, mClasses.All())
    {
      classDoc->Build();
      mClassMap.Insert(classDoc->GenerateMapKey(), classDoc);
    }
    Zero::Sort(mClasses.All(), DocComparePtrFn<RawClassDoc* >);

  }

  void RawDocumentationLibrary::Serialize(Serializer& stream)
  {
    SerializeName(mClassPaths);
    SerializeName(mEnums);
    SerializeName(mFlags);
  }

  bool RawDocumentationLibrary::SaveToFile(StringParam absPath)
  {
    Status status;

    TextSaver saver;
    saver.Open(status, absPath.c_str());

    if (status.Failed())
    {
      Error(status.Message.c_str());
      return false;
    }

    saver.StartPolymorphic("Doc");
    saver.SerializeField("RawDocumentationLibrary", *this);

    saver.Close();
    return true;
  }

  bool RawDocumentationLibrary::LoadFromDocumentationDirectory(StringParam directory)
  {
    WriteLog("loading raw documentation library from directory: %s\n\n", directory.c_str());

    String docLibFile = BuildString(directory, "/", "Library", ".data");

    // loads from the data file and checks types while loading
    if (!LoadFromDataFile(*this, docLibFile, DataFileFormat::Text, true))
      return false;

    // builds path to classes from paths stored in library file and loads them
    for (uint i = 0; i < mClassPaths.Size(); ++i)
    {
      String libPath = BuildString(directory, mClassPaths[i]);

      if (mIgnoreList.DirectoryIsOnIgnoreList(libPath))
        continue;

      RawClassDoc* newClass = new RawClassDoc;
      newClass->mParentLibrary = this;
      mClasses.PushBack(newClass);

      if (!LoadFromDataFile(*newClass, libPath, DataFileFormat::Text, true))
        return false;

      // we had to load the file to get the name before checking its name to ignore it
      if (mIgnoreList.NameIsOnIgnoreList(newClass->mName))
      {
        mClasses.PopBack();
        delete newClass;
      }
    }

    Build();

    printf("done loading raw documentation library");

    return true;
  }

  bool RawDocumentationLibrary::LoadFromDoxygenDirectory(StringParam doxyPath)
  {
    WriteLog("Loading Classes from Doxygen Class XML Files at: %s\n\n", doxyPath.c_str());

    MacroDatabase::GetInstance()->mDoxyPath = doxyPath;

    Array<String> classFilepaths;

    GetFilesWithPartialName(doxyPath, "class_", mIgnoreList, &classFilepaths);
    GetFilesWithPartialName(doxyPath, "struct_", mIgnoreList, &classFilepaths);

    // for each filepath
    for (uint i = 0; i < classFilepaths.Size(); ++i)
    {
      String& filepath = classFilepaths[i];

      // open the doxy file
      TiXmlDocument doc;
      if (!doc.LoadFile(filepath.c_str()))
      {
        WriteLog("ERROR: unable to load file at: %s\n", filepath.c_str());

        continue;
      }

      // get the class name
      TiXmlElement* compoundName = doc.FirstChildElement(gElementTags[eDOXYGEN])
        ->FirstChildElement(gElementTags[eCOMPOUNDDEF])->FirstChildElement("compoundname");

      String className = GetTextFromAllChildrenNodesRecursively(compoundName);

      if (mIgnoreList.NameIsOnIgnoreList(className))
      {
        continue;
      }

      TypeTokens tokens;

      AppendTokensFromString(DocLangDfa::Get(), className, &tokens);

      className = tokens.Back().mText;
      
      RawClassDoc* newClass = AddNewClass(className);

      newClass->LoadFromXmlDoc(&doc, doxyPath, classFilepaths[i]);
    }

    if (mClasses.Size() != 0)
    {
      printf("\n...Done Loading Classes from Doxygen Class XML Files\n\n");
      printf("\nExpanding and parsing any macros found in Doxygen XML Files...\n\n");
      MacroDatabase::GetInstance()->ProcessMacroCalls();
      printf("\n...Done processing macros found in Doxygen XML Files\n\n");

      return true;
    }
    return false;
  }
  
  void RawDocumentationLibrary::LoadAllEnumDocumentationFromDoxygen(StringParam doxyPath)
  {
    WriteLog("Loading Enums from Doxygen namespace XML Files at: %s\n\n", doxyPath.c_str());

    // for every namespace doxy file
    Array<String> namespaceFilepaths;

    String justSystems = FilePath::Combine(doxyPath, "Systems");

    GetFilesWithPartialName(justSystems, "namespace", mIgnoreList, &namespaceFilepaths);

    forRange(String& filepath, namespaceFilepaths.All())
    {
      // load the file
      TiXmlDocument macroFile;

      if (!macroFile.LoadFile(filepath.c_str()))
      {
        Error("unable to load file '%s", filepath.c_str());
      }


      // for each filepath
      forRange(String& filename, namespaceFilepaths.All())
      {
        // open the doxy file
        TiXmlDocument doc;
        if (!doc.LoadFile(filename.c_str()))
        {
          // print error on failure to load
          WriteLog("ERROR: unable to load file at: %s\n", filename.c_str());
          continue;
        }

        // get doxygen node
        // get compounddef node
        TiXmlElement* namespaceDef = doc.FirstChildElement(gElementTags[eDOXYGEN])
          ->FirstChildElement(gElementTags[eCOMPOUNDDEF]);

        TiXmlNode* firstSectDef = GetFirstNodeOfChildType(namespaceDef, gElementTags[eSECTIONDEF]);
        TiXmlNode* endSectDef = GetEndNodeOfChildType(namespaceDef, gElementTags[eSECTIONDEF]);

        TiXmlElement* funcSection = nullptr;

        // find the function section
        for (TiXmlNode* node = firstSectDef; node != endSectDef; node = node->NextSibling())
        {
          TiXmlElement* element = node->ToElement();

          // kind is always first attribute for sections
          TiXmlAttribute* attrib = element->FirstAttribute();

          if (attrib && strcmp(attrib->Value(), "func") == 0)
          {
            funcSection = element;
            break;
          }
        }
        // now that we find the function section, look for function with "DeclareEnum"
        if (!funcSection)
          continue;

        // for each function
        for (TiXmlNode* funcNode = funcSection->FirstChild();
          funcNode != nullptr; funcNode = funcNode->NextSibling())
        {
          TiXmlElement* funcAsElement = funcNode->ToElement();

          TiXmlNode* fnNameNode = GetFirstNodeOfChildType(funcAsElement, gElementTags[eNAME]);

          if (fnNameNode)
          {
            String functionName = fnNameNode->FirstChild()->Value();

            if (functionName.Contains("DeclareEnum") || functionName.Contains("DeclareBitfield"))
            {
              // get the first param because it should have the enum/bitfield's name
              // paramNode->typenode->textnode->value
              String enumName = GetFirstNodeOfChildType(funcAsElement, gElementTags[ePARAM])
                ->FirstChild()->FirstChild()->Value();

              if (!mEnumAndFlagMap.ContainsKey(enumName))
              {
                continue;
              }

              EnumDoc* enumDocToFill = mEnumAndFlagMap[enumName];

              // get the description
              TiXmlNode* descNode = GetFirstNodeOfChildType(funcAsElement, gElementTags[eBRIEFDESCRIPTION]);

              if (!descNode || !descNode->FirstChild())
              {
                continue;
              }

              // now we see if there is a brief description to load
              StringBuilder descriptionBuilder;

              TiXmlNode* descIterNode;

              for (descIterNode = descNode->FirstChild()->FirstChild();
                descIterNode != nullptr && strcmp(descIterNode->Value(),"parameterlist") != 0;
                descIterNode = descIterNode->NextSibling())
              {
                String nodeVal = descIterNode->Value();
                if (nodeVal == "ref")
                {
                  descriptionBuilder << " " << descIterNode->FirstChild()->Value() << " ";
                }
                else
                {
                  descriptionBuilder << descIterNode->Value();
                }
              }

              String description = descriptionBuilder.ToString();

              if (description.Empty() || description == "para")
                continue;

              enumDocToFill->mDescription = description.Trim();

              const String paramNameList = "paramnamelist";
              const String parameterDescription = "parameterdescription";

              //accessing: para->text->paramList
              TiXmlNode* paramList = descIterNode;

              if (!paramList || String("parameterlist") != paramList->Value())
                continue;

              //Node Structure:
              //para
              //  parameterlist
              //    parameteritem
              //      paramnamelist
              //        paramname (child actually has name text)
              //      parameterdescription
              //       para (Child contains actual description)

              // loop over every parameter item
              for(TiXmlNode* node = paramList->FirstChild(); 
                node != nullptr; node = node->NextSibling())
              {
                // paramnamelist->paramname->text
                String value = node->FirstChild()->FirstChild()->FirstChild()->Value();

                if (enumDocToFill->mEnumValues.Contains(value))
                {
                  String valDescription = GetTextFromAllChildrenNodesRecursively(node->FirstChild()->NextSibling()->FirstChild()).Trim();
                  //accessing paramnamelist->paramdescript->para->text
                  enumDocToFill->mEnumValues.InsertOrAssign(value, valDescription);
                }
              }
            }
          }
        }
      }
    }
  }

  bool RawDocumentationLibrary::LoadFromSkeletonFile(StringParam doxyPath,
    const DocumentationLibrary &library)
  {
    MacroDatabase *macroDb = MacroDatabase::GetInstance();

    macroDb->mDoxyPath = doxyPath;

    mEnums = library.mEnums;
    mFlags = library.mFlags;

    forRange(EnumDoc* enumDoc, mEnums.All())
    {
      mEnumAndFlagMap[enumDoc->mName] = enumDoc;
    }
    forRange(EnumDoc* flagDoc, mFlags.All())
    {
      mEnumAndFlagMap[flagDoc->mName] = flagDoc;
    }
    // first add all the classes by name to the library
    forRange(ClassDoc* classDoc, library.mClasses.All())
    {
      String& name = classDoc->mName;
      // if we have already documented this, skip it
      if (mClassMap.ContainsKey(name)
        || mIgnoreList.NameIsOnIgnoreList(name)
        || mBlacklist.isOnBlacklist(name)
        || mBlacklist.isOnBlacklist(classDoc->mBaseClass))
        continue;

      RawClassDoc *newClassDoc = AddNewClass(name);

      newClassDoc->LoadFromSkeleton(*classDoc);

      if (newClassDoc->mImportDocumentation)
        newClassDoc->LoadFromDoxygen(doxyPath);

      // if we are a tool, load our xml description for commands
      newClassDoc->LoadToolXmlInClassDescIfItExists();
    }

    if (mClasses.Size() != 0)
    {
      printf("\n...Done Loading Classes from Doxygen Class XML Files\n\n");
      printf("\nExpanding and parsing any macros found in Doxygen XML Files...\n\n");
      MacroDatabase::GetInstance()->ProcessMacroCalls();
      printf("\n...Done processing macros found in Doxygen XML Files\n\n");

      return true;
    }
    return false;
  }

  void RawDocumentationLibrary::LoadIgnoreList(StringParam absPath)
  {
    if (!LoadFromDataFile(mIgnoreList, absPath, DataFileFormat::Text, true))
      Error("Unable to load ignore list: %s\n", absPath.c_str());
  }

  void RawDocumentationLibrary::LoadEventsList(StringParam absPath)
  {
    Status status;
    DataTreeLoader loader;

    if (!loader.OpenFile(status, absPath))
    {
        Error("Unable to load events list: %s\n", absPath.c_str());
    }
    
    // this gets the unnamed object containing events array
    PolymorphicNode dummyNode;
    loader.GetPolymorphic(dummyNode);

    // gets the events array
    PolymorphicNode listNode;
    //loader.GetPolymorphic(listNode);

    loader.SerializeField("Events", mEvents.mEvents);

    loader.Close();

    printf("...successfully loaded eventList from file...\n");
  }

  void RawDocumentationLibrary::SaveEventListToFile(StringParam absPath)
  {
    mEvents.Sort();
    CreateDirectoryAndParents(absPath.SubString(absPath.Begin(), absPath.FindLastOf('\\').Begin()));

    Status status;

    TextSaver saver;

    saver.Open(status, absPath.c_str());

    if (status.Failed())
    {
      Error("Failed to open file to save raw events list at location : %s\n", absPath.c_str());
      WriteLog("Failed to open file to save raw events list at location : %s\n", absPath.c_str());
      return;
    }

    saver.StartPolymorphic("Doc");
    saver.SerializeField("EventDocList", mEvents);
    saver.EndPolymorphic();
    saver.Close();
  }

  RawClassDoc *RawDocumentationLibrary::GetClassByName(StringParam name,Array<String> &namespaces)
  {
    if (mClassMap.ContainsKey(name))
      return mClassMap[name];

    forRange(String& nameSpace, namespaces.All())
    {
      String newKey = BuildString(nameSpace, name);

      if (mClassMap.ContainsKey(newKey))
        return mClassMap[newKey];

    }
    return nullptr;
  }

  ////////////////////////////////////////////////////////////////////////
  // RawVariableDoc
  ////////////////////////////////////////////////////////////////////////
  RawVariableDoc::RawVariableDoc(void) 
  {
    mTokens = new TypeTokens; 
    mProperty = false;
    mReadOnly = false;
    mStatic = false;
  }

  void RawVariableDoc::LoadFromDoxygen(TiXmlElement* element)
  {
    // Save the name if we are filling a blank Variable doc
    if (mName.Empty())
    {
      mName = GetElementValue(element, gElementTags[eNAME]);

      if (mName.StartsWith("m"))
      {
        mName = mName.SubStringFromByteIndices(1, mName.SizeInBytes());
      }
    }

    // get the description and clean up spaces
    String description = DoxyToString(element, gElementTags[eBRIEFDESCRIPTION]).Trim();
    description = CleanRedundantSpacesInDesc(description);

    if (!description.Empty())
      mDescription = description;

    // if we do not have type tokens yet, grab them from the xml
    if (mTokens->Empty())
    {
      StringBuilder retTypeStr;
      BuildFullTypeString(element, &retTypeStr);

      String retTypeString = retTypeStr.ToString();

      AppendTokensFromString(DocLangDfa::Get(), retTypeString, mTokens);
    }
  }

  RawVariableDoc::~RawVariableDoc(void)
  {
    delete mTokens;
  }

  void RawVariableDoc::Serialize(Serializer& stream)
  {
    SerializeName(mName);
    SerializeName(mDescription);
    SerializeName(mTokens);
  }

  template<> struct Zero::Serialization::Trait<RawVariableDoc>
  {

    enum { Type = StructureType::Object };
    static inline cstr TypeName() { return "RawVariableDoc"; }
  };

  RawVariableDoc::RawVariableDoc(TiXmlElement* element)
  {
    mTokens = new TypeTokens;
    LoadFromDoxygen(element);
    mReadOnly = false;
    mStatic = false;
  }


  ////////////////////////////////////////////////////////////////////////
  // RawShortcutLibrary
  ////////////////////////////////////////////////////////////////////////

  bool RawShortcutLibrary::SaveToFile(StringParam absPath)
  {
    Status status;

    TextSaver saver;

    saver.Open(status, absPath.c_str());
    if (status.Failed())
    {
      Error(status.Message.c_str());
      return false;
    }

    saver.StartPolymorphic("Shortcuts");

    forRange(auto classShortcutPair, mShortcutSets.All())
    {
      saver.StartPolymorphic("ShortcutSetEntry");

      saver.SerializeField("Name", classShortcutPair.first);

      ClassShortcuts* ShortcutSet = classShortcutPair.second;

      saver.StartPolymorphic("var ShortcutSet = Array");

      forRange(RawShortcutEntry& entry, *ShortcutSet)
      {
        saver.StartPolymorphic("ShortcutEntry");
        saver.SerializeField("Index", entry.mIndex);
        saver.SerializeField("Name", entry.mName);
        saver.SerializeField("Shortcut", entry.mShortcut);
        saver.SerializeField("Description", entry.mDescription);
        saver.EndPolymorphic();
      }
      saver.EndPolymorphic();
      saver.EndPolymorphic();
    }

    saver.EndPolymorphic();

    saver.Close();

    return true;
  }
  
  void RawShortcutLibrary::InsertClassShortcuts(StringParam className, ClassShortcuts* shortcuts)
  {
    mShortcutSets.InsertOrAssign(className, shortcuts);
  }

  Zero::ClassShortcuts* RawShortcutLibrary::GetShortcutsForClass(StringParam className)
  {
    return mShortcutSets.FindOrInsert(className);
  }

  ////////////////////////////////////////////////////////////////////////
  // RawTypedefDoc
  ////////////////////////////////////////////////////////////////////////


  // due to a bug in doxygen's typedef documentation,
  // we have to parse definition ourselves
  // NOTE: This should probably switch to just using the token parser
  RawTypedefDoc::RawTypedefDoc(TiXmlElement* element)
  {
    LoadFromElement(element);
  }

  void RawTypedefDoc::LoadFromElement(TiXmlElement* element)
  {
    // we are going to want the name and the type
    mType = GetElementValue(element, gElementTags[eNAME]);

    TiXmlNode* definitionNode = GetFirstNodeOfChildType(element, gElementTags[eDEFINITION]);

    String defString = definitionNode->ToElement()->GetText();

    // lets send this down the tokenizer like an adult instead of what we were doing
    AppendTokensFromString(DocLangDfa::Get(), defString, &mDefinition);

    // get rid of the first token that just says 'typedef'
    mDefinition.PopFront();

    // now find the token that has the name of the typedef in it and remove it
    for (uint i = mDefinition.Size() - 1; i >= 0; --i)
    {
      if (mDefinition[i].mText == mType)
      {
        mDefinition.EraseAt(i);
        break;
      }
    }
  }

  String RawTypedefDoc::GenerateMapKey(void)
  {
    StringBuilder builder;

    for (uint i = 0; i < mNamespace.mNames.Size(); ++i)
    {
      builder.Append(mNamespace.mNames[i]);
    }

    builder.Append(mType);

    return builder.ToString();
  }

  void RawTypedefDoc::Serialize(Serializer& stream)
  {
    SerializeNameDefault(mType, String(""));
    SerializeNameDefault(mDefinition, TypeTokens());
    SerializeName(mNamespace);
  }

  template<> struct Zero::Serialization::Trait<RawTypedefDoc>
  {

    enum { Type = StructureType::Object };
    static inline cstr TypeName() { return "RawTypedefDoc"; }
  };

  ////////////////////////////////////////////////////////////////////////
  // EnumDoc
  ////////////////////////////////////////////////////////////////////////

  void LoadEnumFromDoxy(EnumDoc& enumDoc, TiXmlElement* element, TiXmlNode* enumDef)
  {
    enumDoc.mName = GetElementValue(element, gElementTags[eNAME]);

    // unnamed enums are automatically given a name by doxygen, remove it
    if (enumDoc.mName.FindFirstOf('@') != "")
      enumDoc.mName = "";

    TiXmlNode* DescNode = GetFirstNodeOfChildType(element, "briefdescription");

    if (DescNode)
    {
      TiXmlElement* descEle = DescNode->FirstChildElement();
      if (descEle)
      {
        if (descEle->Type() == TiXmlNode::TEXT)
        {
          enumDoc.mDescription = descEle->GetText();
        }
        else
        {
          StringBuilder desc;
          GetTextFromChildrenNodes(descEle, &desc);
          enumDoc.mDescription = desc.ToString();
        }
      }
      enumDoc.mDescription = enumDoc.mDescription.Trim();
      enumDoc.mDescription = CleanRedundantSpacesInDesc(enumDoc.mDescription);
    }

    // we use these for iteration
    TiXmlNode* firstElement = GetFirstNodeOfChildType(element, "enumvalue");
    TiXmlNode* endNode = GetEndNodeOfChildType(element, "enumvalue");

    int i = 0;
    for (TiXmlNode* node = firstElement; node != endNode; node = node->NextSibling(), ++i)
    {
      // TODO: Actually parse for enumvalue descriptions
      enumDoc.mEnumValues.FindOrInsert(node->FirstChildElement()->GetText(),"");
    }

  }
  /*
  void RawEnumDoc::Serialize(Serializer& stream)
  {
    SerializeNameDefault(mName, String(""));

    SerializeNameDefault(mDescription, String(""));

    SerializeNameDefault(mEnumValues, Array<String>());
  }

  template<> struct Zero::Serialization::Trait<RawEnumDoc>
  {

    enum { Type = StructureType::Object };
    static inline cstr TypeName() { return "RawEnumDoc"; }
  };*/

  ////////////////////////////////////////////////////////////////////////
  // RawClassDoc
  ////////////////////////////////////////////////////////////////////////
  //ZeroDefineType(RawClassDoc);
  ZilchDefineType(RawClassDoc, builder, type)
  {

  }

  RawClassDoc::RawClassDoc(void)
    : mHasBeenLoaded(false), mImportDocumentation(true)
  {
  }

  RawClassDoc::RawClassDoc(StringParam name) 
    : mName(name)
    , mHasBeenLoaded(false)
    , mImportDocumentation(true)
  {
  }

  RawClassDoc::~RawClassDoc()
  {
    forRange(RawMethodDoc* methodDoc, mMethods.All())
    {
      delete methodDoc;
    }

    forRange(RawVariableDoc* variableDoc, mVariables.All())
    {
      delete variableDoc;
    }

    forRange(RawTypedefDoc* typedefDoc, mTypedefs.All())
    { 
      delete typedefDoc;
    }
  }

  void RawClassDoc::LoadFromSkeleton(const ClassDoc &skeleClass)
  {
    mBaseClass = skeleClass.mBaseClass;

    mTags = skeleClass.mTags;

    mLibrary = skeleClass.mLibrary;

    mImportDocumentation = skeleClass.mImportDocumentation;

    if (mLibrary == "Core")
    {
      mRelativePath = BuildString("\\BaseZilchTypes\\", skeleClass.mName, ".data");
    }

    forRange(PropertyDoc* prop, skeleClass.mProperties.All())
    {
      // TODO: add a thing stating we have swizzle operators
      if (NameIsSwizzleOperation(prop->mName))
        continue;

      RawVariableDoc* newVar = new RawVariableDoc();

      newVar->mName = prop->mName;
      newVar->mDescription = prop->mDescription;

      newVar->mReadOnly = prop->mReadOnly;
      newVar->mStatic = prop->mStatic;

      newVar->mTokens = new TypeTokens;
      newVar->mTokens->PushBack(DocToken(prop->mType));

      newVar->mProperty = true;
      mVariables.PushBack(newVar);
    }

    forRange(MethodDoc* meth, skeleClass.mMethods.All())
    {
      // TODO: add a thing stating we have swizzle operators
      if (NameIsSwizzleOperation(meth->mName))
        continue;

      RawMethodDoc* newMethod = new RawMethodDoc();

      newMethod->mName = meth->mName;
      newMethod->mDescription = meth->mDescription;

      newMethod->mStatic = meth->mStatic;

      newMethod->mReturnTokens = new TypeTokens();
      AppendTokensFromString(DocLangDfa::Get(), meth->mReturnType, newMethod->mReturnTokens);

      uint paramIndex = 0;
      forRange(ParameterDoc* param, meth->mParameterList.All())
      {
        RawMethodDoc::Parameter* newParam = new RawMethodDoc::Parameter();

        newParam->mTokens = new TypeTokens();

        if (this->mImportDocumentation == false)
        {
          if (param->mName == "")
          {
            newParam->mName = String::Format("p%d",paramIndex);
          }
          else
          {
            newParam->mName = param->mName;
          } 
          newParam->mDescription = param->mDescription;
        }

        AppendTokensFromString(DocLangDfa::Get(), param->mType, newParam->mTokens);

        newMethod->mParsedParameters.PushBack(newParam);

        ++paramIndex;
      }

      // now that we have properly formatted this, we can make sure it wasn't redundant

      bool alreadyExists = false;
      bool existingMethodHadAny = false;
      bool newMethodHadAny = false;
      RawMethodDoc* oldMethod = nullptr;

      if (mMethodMap.ContainsKey(newMethod->mName))
      {
        forRange(RawMethodDoc*methodDoc, mMethodMap[newMethod->mName].All())
        {
          if (methodDoc->MethodHasSameSignature(*newMethod, &existingMethodHadAny, &newMethodHadAny))
          {
            alreadyExists = true;

            oldMethod = methodDoc;

            break;
          }
        }
      }
      // if the method name does not exist and this new method does not have any
      if (!alreadyExists || !newMethodHadAny)
      {
        mMethods.PushBack(newMethod);
        mMethodMap[newMethod->mName].PushBack(newMethod);
      }
      // new method had any and the method name already existed
      else
      {
        forRange(RawMethodDoc* savedMethods, mMethods.All())
        {
          delete savedMethods;
        }

        mMethods.Clear();

        mMethods.PushBack(newMethod);

        mMethodMap.Clear();

        // rebuild method map
        forRange(RawMethodDoc* methodDoc, mMethods.All())
        {
          mMethodMap[methodDoc->mName].PushBack(methodDoc);
        }

      }
    }

    Build();
  }

  template<> struct Zero::Serialization::Trait<RawClassDoc>
  {

    enum { Type = StructureType::Object };
    static inline cstr TypeName() { return "RawClassDoc"; }
  };

  void RawClassDoc::Serialize(Serializer& stream)
  {
    SerializeName(mName);
    SerializeName(mNamespace);
    SerializeName(mLibrary);
    SerializeName(mRelativePath);
    SerializeName(mHeaderFile);
    SerializeName(mBodyFile);
    SerializeName(mBaseClass);
    SerializeName(mDescription);
    SerializeName(mTypedefs);
    SerializeName(mEvents);
    SerializeName(mVariables);
    SerializeName(mMethods);
  }

  bool RawClassDoc::SaveToFile(StringParam absPath)
  {
    Status status;

    TextSaver saver;

    saver.Open(status, absPath.c_str());
    if (status.Failed())
    {
      Error(status.Message.c_str());
      return false;
    }
    

    saver.StartPolymorphic("Doc");

    saver.SerializeField("RawClassDoc", *this);

    saver.Close();

    return true;
  }

  // we could change this to take a bool whether to override or not
  void RawClassDoc::Add(RawClassDoc& classDoc)
  {
    const Array<RawMethodDoc* > empty;
    // for every method in the passed in class documentation
    forRange(RawMethodDoc* methodDoc, classDoc.mMethods.All())
    {
      // get the array of methods by the same name we have (if any)
      const Array<RawMethodDoc* >& sameNames
        = mMethodMap.FindValue(methodDoc->mName, empty);
      
      // if we had any methods by the same name, check if they have the
      // same signature before we add them
      bool sameSigExists = false;
      forRange(RawMethodDoc* sameNamedMeth, sameNames.All())
      {
        if (sameNamedMeth->mReturnTokens == methodDoc->mReturnTokens 
          && sameNamedMeth->mParsedParameters.Size() == methodDoc->mParsedParameters.Size())
        {
          for (uint i = 0; i < sameNamedMeth->mParsedParameters.Size(); ++i)
          {
            // check if this is the same parameter type by comparing token lists
            bool sameParam = *(sameNamedMeth->mParsedParameters[i]->mTokens) == *(methodDoc->mParsedParameters[i]->mTokens);

            if (!sameParam)
            {
              sameSigExists = false;
              break;
            }
          }

          if (sameSigExists)
          {
            // same function already documented, check if it needs a description before breaking
            if (sameNamedMeth->mDescription.Empty())
            {
              sameNamedMeth->mDescription = methodDoc->mDescription;
            }

            // just break, once we find one, the rest of the methods don't matter
            break;
          }
        }
      }
      
      if (!sameSigExists)
        mMethods.PushBack(methodDoc);
    }

    // for every property in the passed in class documentation
    forRange(RawVariableDoc* propertyDoc, mVariables.All())
    {
      // if we didn't find a property by that name, add it
      if (!mVariableMap.FindValue(propertyDoc->mName, nullptr))
      {
        mVariables.PushBack(propertyDoc);
      }
    }
    // since we just added data (presumably) to both of these, time to clear and build
    Rebuild();
  }

  void RawClassDoc::Build(void)
  {
    mMethodMap.Clear();
    mVariableMap.Clear();
    // sort the method and property Arrays
    Zero::Sort(mMethods.All(), DocComparePtrFn<RawMethodDoc* >);
    Zero::Sort(mVariables.All(), DocComparePtrFn<RawVariableDoc* >);

    // build method map
    forRange(RawMethodDoc* methodDoc, mMethods.All())
    {
      mMethodMap[methodDoc->mName].PushBack(methodDoc);
    }

    // build property map
    forRange(RawVariableDoc* propertyDoc, mVariables.All())
    {
      mVariableMap[propertyDoc->mName] = propertyDoc;
    }

  }

  // same as build but clears everything first
  void RawClassDoc::Rebuild(void)
  {
    mMethodMap.Clear();
    mVariableMap.Clear();

    Build();
  }

  void RawClassDoc::FillTrimmedClass(ClassDoc* trimClass)
  {
    Rebuild();

    trimClass->mName = mName;

    trimClass->mBaseClass = mBaseClass;
    trimClass->mDescription = mDescription;
    trimClass->mEventsSent = mEvents;
    trimClass->mTags = mTags;
    trimClass->mLibrary = mLibrary;

    for (uint i = 0; i < trimClass->mEventsSent.Size(); ++i)
    {
      EventDoc *eventDoc = trimClass->mEventsSent[i];
      trimClass->mEventsMap[eventDoc->mName] = eventDoc;
    }

    // pull out all variables as properties (Removing leading 'm' where exists)
    for (uint i = 0; i < mVariables.Size(); ++i)
    {
      RawVariableDoc* rawVar = mVariables[i];

      if (!rawVar->mProperty)
        continue;

      PropertyDoc* trimProp = new PropertyDoc();

      trimProp->mReadOnly = rawVar->mReadOnly;
      trimProp->mStatic = rawVar->mStatic;

      // trim off the leading 'm' if it has one
      if (rawVar->mName.c_str()[0] == 'm')
        trimProp->mName = rawVar->mName.SubStringFromByteIndices(1, rawVar->mName.SizeInBytes());
      else
        trimProp->mName = rawVar->mName;

      trimProp->mDescription = rawVar->mDescription;

      trimProp->mType = TrimTypeTokens(*rawVar->mTokens);

      trimClass->mPropertiesMap[trimProp->mName] = trimProp;
    }

    // for each method
    for (uint i = 0; i < mMethods.Size(); ++i)
    {
      if (mMethods[i]->mReturnTokens->Empty())
        continue;

      // first unpack all the information
      MethodDoc* trimMethod = new MethodDoc;
      mMethods[i]->FillTrimmedMethod(trimMethod);

      // if this is a really short method name there is no point to check for a prefix
      if (trimMethod->mName.ComputeRuneCount() < 4)
      {
        trimClass->mMethods.PushBack(trimMethod);
      }
      else
      {
        String firstThreeChar = trimMethod->mName.SubStringFromByteIndices(0, 3).ToLower();

        // is it a getter?
        if (firstThreeChar == "get")
        {
          StringRange propName = trimMethod->mName.SubStringFromByteIndices(3, trimMethod->mName.SizeInBytes());

          // check if a corresponding property already exists (this should override prop doc if it exists)
          if (trimClass->mPropertiesMap.ContainsKey(propName))
          {
            // if prop exists, make sure we are returning the same type
            PropertyDoc *prop = trimClass->mPropertiesMap[propName];

            if (!trimMethod->mDescription.Empty())
            {
              prop->mDescription = trimMethod->mDescription;
            }
          }
          else
          {
            trimClass->mMethods.PushBack(trimMethod);
          }

        }
        // 'is' functions as a getter for a boolean property
        else if (trimMethod->mName.SubStringFromByteIndices(0, 2).ToLower() == "is")
        {
          StringRange propName = trimMethod->mName.SubStringFromByteIndices(2, trimMethod->mName.SizeInBytes());

          // check if a corresponding property already exists
          if (trimClass->mPropertiesMap.ContainsKey(propName)
            && trimClass->mPropertiesMap[propName]->mType == trimMethod->mReturnType)
          {
            // if prop exists, make sure we are returning the same type
            // NOTE: I am going to ignore differences in const or & since it is the same
            PropertyDoc *prop = trimClass->mPropertiesMap[propName];

            // override getter over existing property if getter description exists
            if (!trimMethod->mDescription.Empty())
            {
              prop->mDescription = trimMethod->mDescription;
            }

          }
          else
          {
            trimClass->mMethods.PushBack(trimMethod);
          }
        }
        // is it a setter?
        else if (firstThreeChar == "set")
        {
          StringRange propName = trimMethod->mName.SubStringFromByteIndices(3, trimMethod->mName.SizeInBytes());

          // we only care if this is already a property and has no description
          // we can do this since methods will be in alphebetical order so
          // all the 'set's with come after the 'is' and 'get's
          if (trimClass->mPropertiesMap.ContainsKey(propName))
          {
            if (trimClass->mPropertiesMap[propName]->mDescription.Empty())
              trimClass->mPropertiesMap[propName]->mDescription = trimMethod->mDescription;
          }
          else
          {
            trimClass->mMethods.PushBack(trimMethod);
          }
        }
        else
        {
          trimClass->mMethods.PushBack(trimMethod);
        }
      }
    }

    forRange(auto prop, trimClass->mPropertiesMap.All())
    {
      trimClass->mProperties.PushBack(prop.second);
    }
  }

  bool RawClassDoc::LoadFromDoxygen(StringParam doxyPath)
  {
    TiXmlDocument doc;

    bool loaded = loadDoxyfile(mName, doxyPath, doc, false);
    // load the doxy xml file into a tinyxml document
    if (!loaded && !mParentLibrary->mZilchTypeToCppClassList.ContainsKey(mName))
    {
      return false;
    }

    if (loaded)
    {
      if (!LoadFromXmlDoc(&doc))
        return false;
    }
    // check our map for zilch types to cpp types to try to find a doxy file with this type
    forRange(String& cppName, mParentLibrary->mZilchTypeToCppClassList[mName].All())
    {
      if (loadDoxyfile(cppName, doxyPath, doc, false))
      {
        if (LoadFromXmlDoc(&doc))
        {
          loaded = true;
        }
      }
    }

    LoadEvents(GetDoxygenName(mName), doxyPath);
    
    return true;
  }

  // param number technically starts at 1 since 0 means no param for this fn
  bool FillEventInformation(StringParam fnTokenName, uint paramNum, bool listeningFn, 
    EventDocList &libEventList, RawClassDoc *classDoc, TypeTokens &tokens)
  {
    if (tokens.Contains(DocToken(fnTokenName)))
    {
      uint i = 0;
      uint commaCount = 0;
      for (; i < tokens.Size() && commaCount < paramNum; ++i)
      {
        if (tokens[i].mEnumTokenType == DocTokenType::Comma)
        {
          ++commaCount;
        }
      }

      if (commaCount == paramNum)
      {
        // i - 2 because i is currently at 1 past the comma index
        String& eventName = tokens[i - 2].mText;

        EventDoc *eventDoc = nullptr;

        if (libEventList.mEventMap.ContainsKey(eventName))
        {
          eventDoc = libEventList.mEventMap[eventName];
          if (listeningFn)
          {
            eventDoc->mListeners.PushBack(classDoc->mName);
            classDoc->mEventsListened.PushBack(eventDoc);
          }
          else
          {
            eventDoc->mSenders.PushBack(classDoc->mName);
            classDoc->mEventsSent.PushBack(eventDoc);
          }
          return true;
        }
      }
    }

    return false;
  }

  String buildExceptionText(StringParam exceptionName, StringParam exceptionText)
  {
    StringBuilder builder;

    forRange(Rune c, exceptionName.All())
    {
      if (c != '"')
        builder.Append(c);
    }

    if (!exceptionName.Empty() && !exceptionText.Empty())
      builder.Append(": ");

    forRange(Rune c, exceptionText.All())
    {
      if (c != '"')
        builder.Append(c);
    }

    return builder.ToString();
  }

  void RawClassDoc::AddIfNewException(StringParam fnName, ExceptionDoc *errorDoc)
  {
    RawMethodDoc *currFn = mMethodMap[fnName][0];

    forRange(ExceptionDoc *doc, currFn->mPossibleExceptionThrows.All())
    {
      if (doc->mTitle == errorDoc->mTitle && doc->mMessage == errorDoc->mMessage)
      {
        delete errorDoc;
        return;
      }
    }

    mMethodMap[fnName][0]->mPossibleExceptionThrows.PushBack(errorDoc);
  }

  void RawClassDoc::LoadToolXmlInClassDescIfItExists(void)
  {
    StringRange xmlStart = mDescription.FindFirstOf("<Commands");

    // if this does not contains the start of a macro tag skip it
    if (!xmlStart.SizeInBytes())
    {
      return;
    }

    StringRange xmlEnd = mDescription.FindFirstOf("</Commands>");

    String toolCommandsXmlString = mDescription.SubString(xmlStart.Begin(), xmlEnd.End());

    // if this is true we either have no description or the description is after the xml
    if (xmlEnd.End() == mDescription.End())
    {
      // cut the macro out of the description or just remove description if no comment existed
      mDescription = mDescription.SubString(mDescription.Begin(), xmlStart.Begin());
    }
    else
    {
      // cut macro out of description
      mDescription = mDescription.SubString(xmlEnd.End(), mDescription.End());
    }

    TiXmlDocument toolCommandsXml;

    toolCommandsXml.Parse(toolCommandsXmlString.c_str());

    TiXmlElement* macroElement = toolCommandsXml.FirstChildElement();

    ClassShortcuts *classShortcuts = new ClassShortcuts();

    uint commandIndex = 0;
    // unpack all child nodes and save them as options
    for (TiXmlElement* command = macroElement->FirstChildElement();
      command != nullptr; command = command->NextSiblingElement())
    {
      RawShortcutEntry& newShortcutDoc = classShortcuts->PushBack();

      newShortcutDoc.mIndex = commandIndex;

      ++commandIndex;

      const char *name = command->Attribute("name");

      newShortcutDoc.mName = name;

      // if the name was empty, log that
      if (!name)
      {
        WriteLog("Warning: Class '%s' is mssing a command name in command xml documentation.\n", mName.c_str());
      }

      // get shortcut
      TiXmlElement* shortcut = command->FirstChildElement();

      if (!shortcut)
      {
        WriteLog("Error: Class '%s missing shortcut element in command '%s'.\n", mName.c_str(), name);
        break;
      }

      newShortcutDoc.mShortcut = GetTextFromAllChildrenNodesRecursively(shortcut);

      // get description
      TiXmlNode* description = shortcut->NextSibling();

      if (!description)
      {
        WriteLog("Error: Class '%s missing description element in command '%s'.\n", mName.c_str(), name);
        break;
      }

      newShortcutDoc.mDescription = description->Value();

      if (newShortcutDoc.mDescription.Empty())
      {
        WriteLog("Class '%s' is missing description for command '%s'", mName.c_str(), name);
      }
    }

    // Add the command doc to the library
    mParentLibrary->mShortcutsLibrary.InsertClassShortcuts(mName.c_str(), classShortcuts);
  }

  // param number technically starts at 1 since 0 means no param for this fn
  // need to give this some newfangled way to detect what function we are in
  void RawClassDoc::FillErrorInformation(StringParam fnTokenName, StringParam fnName, TypeTokens &tokens)
  {
    if (!mMethodMap.ContainsKey(fnName))
    {
      return;
    }

    if (tokens.Contains(DocToken(fnTokenName)))
    {
      String firstParam = GetArgumentIfString(tokens, 1);
      String secondParam = GetArgumentIfString(tokens, 2);

      if (!firstParam.Empty() || !secondParam.Empty())
      {
        ExceptionDoc *errorDoc = new ExceptionDoc();

        String exceptionText = buildExceptionText(firstParam, secondParam);

        errorDoc->mTitle = firstParam.Empty() ? "[variable]" : firstParam;
        errorDoc->mMessage = secondParam.Empty() ? "[variable]" : secondParam;

        //TODO: actually make sure we find the correct overload instead of the first one

        AddIfNewException(fnName, errorDoc);
      }
    }
  }

  void RawClassDoc::LoadEventsFromCppDoc(TiXmlDocument *doc)
  {
    ParseCodelinesInDoc(doc, [](RawClassDoc *classDoc, StringParam codeString) 
    {
      EventDocList &libEventList = classDoc->mParentLibrary->mEvents;

      TypeTokens tokens;

      AppendTokensFromString(DocLangDfa::Get(), codeString, &tokens);

      // the next block is all of the send and receive event fns we want to parse
      // if boolean argument is false, it is a send function, otherwise, receive

      if (FillEventInformation("DispatchEvent", 1, false, libEventList, classDoc, tokens))
      {
      }
      // only found whern we search outside of bound types
      else if (FillEventInformation("CreateCollisionEvent", 3, false, libEventList, classDoc, tokens))
      {
      }
      else if (FillEventInformation("Dispatch", 1, false, libEventList, classDoc, tokens))
      {
      }
      // SendButtonEvent(event, Events::LockStepGamepadUp, false);
      else if (FillEventInformation("SendButtonEvent", 2, false, libEventList, classDoc, tokens))
      {
      }
      // mLockStep->QueueSyncedEvent(Events::LockStepKeyUp, &syncedEvent); (sends)
      else if (FillEventInformation("QueueSyncedEvent", 1, false, libEventList, classDoc, tokens))
      {
      }
      else if (FillEventInformation("Connect", 2, true, libEventList, classDoc, tokens))
      {
      }
      else if (FillEventInformation("ConnectThisTo", 2, true, libEventList, classDoc, tokens))
      {
      }
    });
  }

  void RawClassDoc::LoadEventsFromHppDoc(TiXmlDocument *doc)
  {
    ParseCodelinesInDoc(doc, [](RawClassDoc *classDoc, StringParam codeString) {
      TypeTokens tokens;

      if (!codeString.Empty())
      {
        AppendTokensFromString(DocLangDfa::Get(), codeString, &tokens);

        for (uint i = 0; i < tokens.Size(); ++i)
        {
          // if we found the events namespace node, the node two up from here is event
          if (tokens[i].mText == "Events")
          {
            i += 2;
            EventDoc *eventDoc = new EventDoc;

            classDoc->mEvents.PushBack(eventDoc);

            eventDoc->mName = tokens[i].mText;
            //classDoc->mEvents.PushBack(tokens[i].mText);
            return;
          }
        }
      }
    });
  }

  void RawClassDoc::ParseCodelinesInDoc(TiXmlDocument *doc, void(*fn)(RawClassDoc *, StringParam))
  {
    // grab the class
    TiXmlElement* cppDef = doc->FirstChildElement(gElementTags[eDOXYGEN])
      ->FirstChildElement(gElementTags[eCOMPOUNDDEF]);

    TiXmlElement *programList = GetFirstNodeOfChildType(cppDef, "programlisting")->ToElement();

    TiXmlNode *firstCodeline = GetFirstNodeOfChildType(programList, gElementTags[eCODELINE]);
    TiXmlNode *endCodeline = GetEndNodeOfChildType(programList, gElementTags[eCODELINE]);

    for (TiXmlNode *codeline = firstCodeline;
      codeline != endCodeline;
      codeline = codeline->NextSibling())
    {
      StringBuilder builder;

      GetTextFromAllChildrenNodesRecursively(codeline, &builder);

      String codeString = builder.ToString();

      fn(this, codeString);
    }
  }

  // this function assumes we are not contained within scope
  bool isFunctionDefinition(TypeTokens &tokens)
  {
    if (tokens.Contains(DocToken(";", DocTokenType::Semicolon))
      ||tokens.Contains(DocToken("#", DocTokenType::Pound)))
    {
      return false;
    }

    if (tokens.Contains(DocToken("(", DocTokenType::OpenParen)))
      return true;

    return false;
  }

  String getStringFromFnTokenList(TypeTokens &tokens)
  {
    for (uint i = 0; i < tokens.Size(); ++i)
    {
      DocToken &token = tokens[i];

      if (token.mEnumTokenType == DocTokenType::OpenParen)
      {
        if (i == 0)
        {
          return "";
        }
        return tokens[i - 1].mText;
      }
    }
    return "";
  }

  void RawClassDoc::ParseFnCodelinesInDoc(TiXmlDocument *doc)
  {
    // might want to maintain a static list of files we have already processed so we do
    // not end up doubling up documentation for classes that are implemented in the same
    //file.
    EventDocList &libEventList = mParentLibrary->mEvents;

    // grab the class
    TiXmlElement* cppDef = doc->FirstChildElement(gElementTags[eDOXYGEN])
      ->FirstChildElement(gElementTags[eCOMPOUNDDEF]);

    TiXmlElement *programList = GetFirstNodeOfChildType(cppDef, "programlisting")->ToElement();

    TiXmlNode *firstCodeline = GetFirstNodeOfChildType(programList, gElementTags[eCODELINE]);
    TiXmlNode *endCodeline = GetEndNodeOfChildType(programList, gElementTags[eCODELINE]);

    String currFn = "";
    RawClassDoc *currClass = this;

    Array<String> namespaces;
    namespaces.PushBack("Zero");
    namespaces.PushBack("Zilch");

    // the majority of this loop is just getting the current function
    for (TiXmlNode *codeline = firstCodeline;
      codeline != endCodeline;
      codeline = codeline->NextSibling())
    {
      StringBuilder builder;

      GetTextFromAllChildrenNodesRecursively(codeline, &builder);

      if (builder.GetSize() <= 0)
        continue;

      String codeString = builder.ToString();

      TypeTokens tokens;

      AppendTokensFromString(DocLangDfa::Get(), codeString, &tokens);

      if (tokens.Empty())
        continue;

      // does the current line have a pound? Then just continue.
      if (tokens[0].mEnumTokenType == DocTokenType::Pound)
        continue;

      // have a semicolon?
      if (tokens.Contains(DocToken(";", DocTokenType::Semicolon)))
      {
        // check if it is one of the lines we are looking for
        if (currFn.Empty())
          continue;
        // otherwise, we are going to look for any exceptions, and save it if we find one
        FillErrorInformation("DoNotifyException", currFn, tokens);

        // once we are done, continue
        continue;
      }

      // does it have '::'?
      for (uint i = 0; i < tokens.Size(); ++i)
      {
        DocToken &currToken = tokens[i];

        if (currToken.mEnumTokenType != DocTokenType::ScopeResolution)
          continue;

        DocToken &lhs = tokens[i - 1];
        DocToken &rhs = tokens[i + 1];


        RawClassDoc *docClass = mParentLibrary->GetClassByName(lhs.mText, namespaces);
        // check if whatever is to the left of that is a known classname
        if (docClass)
        {
          currClass = docClass;
        }

        // see if the function exists
        if (currClass->mMethodMap.ContainsKey(rhs.mText))
        {
          currFn = rhs.mText;
        }
        //TODO: in the event of multiple of the '::', do this same check for each one
      }
    }
  }



  void RemoveDuplicates(Array<String> &stringList)
  {
    for (uint i = 1; i < stringList.Size(); ++i)
    {
      // get rid of duplicates
      if (stringList[i] == stringList[i - 1])
      {
        stringList.EraseAt(i);
        --i;
      }
    }

  }

  void RawClassDoc::SortAndPruneEventArray(void)
  {
    EventDocList &libEventList = mParentLibrary->mEvents;

    for (uint i = 0; i < libEventList.mEvents.Size(); ++i)
    {
      Sort(libEventList.mEvents[i]->mSenders.All());
      Sort(libEventList.mEvents[i]->mListeners.All());
      RemoveDuplicates(libEventList.mEvents[i]->mSenders);
      RemoveDuplicates(libEventList.mEvents[i]->mListeners);
    }
    if (mEvents.Empty())
      return;

    Sort(mEvents.All(), [](EventDoc *lhs, EventDoc * rhs)
    {
      return lhs->mName < rhs->mName;
    });

    // we have to get rid of duplicates in case event was defined and bound in same class
    for (uint i = 1; i < mEvents.Size(); ++i)
    {
      // get rid of duplicates
      if (mEvents[i] == mEvents[i - 1])
      {
        mEvents.EraseAt(i);
        --i;
      }
    }
  }

  bool RawClassDoc::SetRelativePath(StringParam doxyPath, StringParam filePath)
  {
    if (filePath.Empty())
    {
      WriteLog("Error: RawClassDoc: %s has empty filepath\n", mName.c_str());
      return false;
    }

    StringRange relPath = filePath.SubStringFromByteIndices(doxyPath.SizeInBytes(), filePath.SizeInBytes());

    relPath = relPath.SubString(relPath.Begin(), relPath.FindLastOf("\\xml\\").Begin());

    if (relPath.SizeInBytes() == 0)
      return false;

    StringBuilder path;

    path.Append(relPath);
    path.Append('\\');

    for (int i = 0; i < (int)mNamespace.mNames.Size() - 1; ++i)
    {
      path.Append(mNamespace.mNames[i]);
      path.Append('\\');
    }
    
    // there is probably a better way to do this but...   
    forRange(Rune c, mName.All())
    {
      if (c == ':' || c == '*' || c == '?' || c == '\"' || c == '<' || c == '>' || c == '|')
        path.Append('_');
      else
        path.Append(c);
    }

    path.Append(".data");
    // we are guarenteed to already have our name so this is safe
    mRelativePath = path.ToString();

    return true;
  }

  bool RawClassDoc::LoadEvents(String doxName, String doxyPath)
  {
    String filename = GetDoxyfileNameFromSourceFileName(mBodyFile);

    filename = GetFileWithExactName(doxyPath, filename);

    if (filename.Empty())
      return false;

    TiXmlDocument cppDoc;

    if (!cppDoc.LoadFile(filename.c_str()))
    {
      WriteLog("Failed to load file: %s\n", filename.c_str());
      // still return true since we did load the class doc
      return false;
    }

    LoadEventsFromCppDoc(&cppDoc);

    ParseFnCodelinesInDoc(&cppDoc);

    // could also load from hpp but it turns out you get everything you need from cpp

    SortAndPruneEventArray();

    return true;
  }

  bool RawClassDoc::LoadFromXmlDoc(TiXmlDocument* doc, StringParam doxyPath,
    StringParam filePath, IgnoreList *ignoreList)
  {
    if (ignoreList && ignoreList->DirectoryIsOnIgnoreList(BuildString(doxyPath,filePath)))
    {
      return false;
    }
    
    if (LoadFromXmlDoc(doc) && SetRelativePath(doxyPath, filePath))
    {
      // check if the class Initializes Meta
      if (mMethodMap.ContainsKey("InitializeMeta"))
      {
        // add path and name to seperatelist of events to find
        String doxName = GetDoxygenName(mName);

        LoadEvents(doxName, doxyPath);
      }

      return true;
    }

    return false;
  }

  bool RawClassDoc::LoadFromXmlDoc(TiXmlDocument* doc)
  {
    // if we already exist in the library, that means we only want to load specific members
    bool onlySaveValidProperties = mParentLibrary != nullptr && mParentLibrary->mClassMap.ContainsKey(mName);

    MacroDatabase *macroDb = MacroDatabase::GetInstance();

    // grab the class
    TiXmlElement* classDef = doc->FirstChildElement(gElementTags[eDOXYGEN])
      ->FirstChildElement(gElementTags[eCOMPOUNDDEF]);

    // get the namespace for the class
    TiXmlElement* compoundName = classDef->FirstChildElement("compoundname");

    String classNamespace = GetTextFromAllChildrenNodesRecursively(compoundName->FirstChild());

    TypeTokens namespaceTokens;
    AppendTokensFromString(DocLangDfa::Get(), classNamespace, &namespaceTokens);

    mNamespace.GetNamesFromTokens(namespaceTokens);

    // grab the base class if we have one
    TiXmlElement* baseClassElement = classDef->FirstChildElement(gElementTags[eBASECOMPOUNDREF]);
    if (baseClassElement != nullptr && mBaseClass.Empty())
    {
      mBaseClass = baseClassElement->GetText();
    }

    // get the mDescription of the class
    String description = DoxyToString(classDef, gElementTags[eBRIEFDESCRIPTION]).Trim();

    // if the new description is not empty and the old one is, copy it over
    if (!description.Empty() && mDescription.Empty())
      mDescription = description;

    // we are going to traverse over every section that the class contains
    for (TiXmlNode* pSection = classDef->FirstChild()
      ; pSection != 0
      ; pSection = pSection->NextSibling())
    {
      // first we need to figure out what kind of section this is

      // sometimes the first section is the mDescription, so test for that
      if (strcmp(pSection->Value(), gElementTags[eBRIEFDESCRIPTION]) == 0)
      {
        description = DoxyToString(pSection->ToElement(), gElementTags[ePARA]).Trim();
        if (!description.Empty() && mDescription.Empty())
          mDescription = description;
      }

      // loop over all members of this section
      for (TiXmlNode* pMemberDef = pSection->FirstChild()
        ; pMemberDef != 0
        ; pMemberDef = pMemberDef->NextSibling())
      {
        TiXmlElement* memberElement = pMemberDef->ToElement();
        // check if it is actually a memberdef
        if (!memberElement
          || strcmp(memberElement->Value(), gElementTags[eMEMBERDEF]) != 0)
          continue;

        // get the kind of element we are (which happens to be the first attrib)
        TiXmlAttribute* kind = memberElement->FirstAttribute();

        // some things don't need strcmp as they have a unique first character
        switch (kind->Value()[0])
        {
        case 'v': // if we are a variable
        {
          String name = GetElementValue(memberElement, gElementTags[eNAME]);
          if (!name.Empty())
            name = name.SubStringFromByteIndices(1, name.SizeInBytes());

          // only save the variable if we are saving all variables or if it is one of our known vals
          if (mVariableMap.ContainsKey(name))
          {
            mVariableMap[name]->LoadFromDoxygen(memberElement);
          }
          else if (!onlySaveValidProperties)
          {
            mVariables.PushBack(new RawVariableDoc(memberElement));
          }
        }
          break;
        case 't': // else if we are a typedef
        {
          RawTypedefDoc* newTd = new RawTypedefDoc(memberElement);

          newTd->mNamespace.mNames = mNamespace.mNames;

          mTypedefs.PushBack(newTd);
          break;
        }
        case 'e': //else if we are an enum
        {
          EnumDoc *enumDoc = new EnumDoc();
          LoadEnumFromDoxy(*enumDoc, memberElement, pMemberDef);
          // if we already had this, remove the new one (TODO: Merge documentation)
          if (mParentLibrary->mEnumAndFlagMap.ContainsKey(enumDoc->mName))
          {
            delete enumDoc;
            break;
          }

          // otherwise, save it to the parent library
          mParentLibrary->mEnums.PushBack(enumDoc);
          mParentLibrary->mEnumAndFlagMap[enumDoc->mName] = enumDoc;

          break;
        }
        case 'f':  // function or friend

          if (strcmp(kind->Value(), "friend") == 0)
          {
            mFriends.PushBack(GetElementValue(memberElement, gElementTags[eNAME]));
            break;
          }
          else // function
          {
            if (mBodyFile.Empty())
            {
              TiXmlNode *locationNode = GetFirstNodeOfChildType(memberElement, "location");

              if (locationNode)
              {
                TiXmlElement *locElement = locationNode->ToElement();

                const char *attString = locElement->Attribute("file");

                if (attString)
                {
                  mHeaderFile = attString;

                  // since this path is going to be from doxygen it will have correct slashes
                  StringRange pos = mHeaderFile.FindLastOf(cDirectorySeparatorChar);
                  pos.IncrementByRune();

                  mHeaderFile = mHeaderFile.SubString(pos.Begin(), mHeaderFile.End());
                }

                attString = locElement->Attribute("bodyfile");

                if (attString)
                {
                  mBodyFile = attString;

                  StringRange pos = mBodyFile.FindLastOf(cDirectorySeparatorChar);

                  pos.IncrementByRune();

                  mBodyFile = mBodyFile.SubString(pos.Begin(), mBodyFile.End());
                }
              }
            }
            String name = GetElementValue(memberElement, gElementTags[eNAME]);

            // if it has no return type and is not a constructor, pass it to macro paser
            if (fnIsMacroCall(memberElement, pMemberDef))
            {
              macroDb->SaveMacroCallFromClass(this, memberElement, pMemberDef);
            }
            else if (mMethodMap.ContainsKey(name))
            {
              FillExistingMethodFromDoxygen(name, memberElement, pMemberDef);
            }
            // check if this is actually a property and we were tricked
            else if (mVariableMap.ContainsKey(name) && name.ToLower().StartsWith("is"))
            {
              // load a temp RawMethodDoc
              RawMethodDoc* tempMethod = new RawMethodDoc(memberElement, pMemberDef);
              RawVariableDoc* newVariable = mVariableMap[name];

              // manually fill it (return type == prop type)
              // extract the name and description
              newVariable->mName = tempMethod->mName;
              newVariable->mDescription = tempMethod->mDescription;
              *(newVariable->mTokens) = *(tempMethod->mReturnTokens);

              // delete the RawMethodDoc (lol)
              delete tempMethod;
            }
            else if (!onlySaveValidProperties)
            {
              mMethods.PushBack(new RawMethodDoc(memberElement, pMemberDef));
            }
            break;
          }

        default: // unreconized tag
          Error("Unreconized Tag of kind: ", kind->Value(), "\n");
        }
      }
    }

    mHasBeenLoaded = true;
    Build();
    mDescription = CleanRedundantSpacesInDesc(mDescription);
    return true;
  }

  bool RawClassDoc::fnIsMacroCall(TiXmlElement* element, TiXmlNode* currMethod)
  {
    String name = GetElementValue(element, gElementTags[eNAME]);
    // this means it is a constructor or deconstructor
    if (name == mName || BuildString("~", mName) == name)
    {
      return false;
    }

    StringBuilder retTypeStr;

    BuildFullTypeString(element, &retTypeStr);

    // if it is empty, that means we are a macro so return true
    return retTypeStr.ToString().Empty();
  }

  bool RawClassDoc::loadDoxyFileReturnHelper(StringParam doxyPath, StringParam fileName)
  {
    if (!SetRelativePath(doxyPath, fileName))
      return false;

    mHasBeenLoaded = true;
    return true;
  }

  bool RawClassDoc::loadClosestDoxyfileMatch(StringParam nameToSearchFor,StringParam doxyPath, TiXmlDocument& doc)
  {
    Array<String> matchingFilesList;
    GetFilesWithPartialName(doxyPath, GetDoxygenName(nameToSearchFor), &matchingFilesList);

    // we are going to load the first 'class_' or 'struct_' file we find
    forRange(String& filePath, matchingFilesList.All())
    {
      // get just the filename
      String endPath = filePath.SubString(filePath.FindLastOf("\\").Begin(), filePath.End());
      if (endPath.Contains("class_") || endPath.Contains("struct"))
      {
        if (doc.LoadFile(filePath.c_str()))
        {

          return loadDoxyFileReturnHelper(doxyPath, filePath);
        }
      }
    }
    return false;
  }

  bool RawClassDoc::loadDoxyfile(StringParam nameToSearchFor, StringParam doxyPath,
    TiXmlDocument& doc, bool isRecursiveCall)
  {
    // try to open the class file
    String fileName = FindFile(doxyPath, BuildString("class_zero_1_1"
      , GetDoxygenName(nameToSearchFor), ".xml"));

    bool loadOkay = doc.LoadFile(fileName.c_str());

    if (loadOkay)
      return loadDoxyFileReturnHelper(doxyPath, fileName);

    // try to open a zilch version of the class file name
    fileName = FindFile(doxyPath, BuildString("class_zilch_1_1"
        , GetDoxygenName(nameToSearchFor), ".xml"));

    loadOkay = doc.LoadFile(fileName.c_str());

    if (loadOkay)
      return loadDoxyFileReturnHelper(doxyPath, fileName);

    // if loading the class file  failed, search for a struct file
    fileName = FindFile(doxyPath, BuildString("struct_zero_1_1"
      , GetDoxygenName(nameToSearchFor).c_str(), ".xml"));

    loadOkay = doc.LoadFile(fileName.c_str());

    if (loadOkay)
      return loadDoxyFileReturnHelper(doxyPath, fileName);

     fileName = FindFile(doxyPath, BuildString("struct_zero_1_1_physics_1_1"
       , GetDoxygenName(nameToSearchFor).c_str(), ".xml"));

     loadOkay = doc.LoadFile(fileName.c_str());

     if (loadOkay)
       return loadDoxyFileReturnHelper(doxyPath, fileName);


     // check known name differences between zilch and c++ types for file names
     if (!nameToSearchFor.Contains("Class"))
     {
       // Try again but with class appended
       if (loadDoxyfile(BuildString(nameToSearchFor, "Class"), doxyPath, doc, true))
       {
         // we don't use the return helper since the recursive call will use it
         return true;
       }
     }

     // if we get here, our filename guesses are not working
     if (loadClosestDoxyfileMatch(nameToSearchFor, doxyPath, doc))
     {
       return true;
     }
     else if (!isRecursiveCall)
     {
       WriteLog("Failed to load doc file for %s\n", mName.c_str());
     }
     return false;
  }

  bool RawClassDoc::loadDoxyfile(StringParam doxyPath, TiXmlDocument& doc, IgnoreList &ignoreList)
  {
    //try to open the class file
    String fileName = FindFile(doxyPath, BuildString("class_zero_1_1"
      , GetDoxygenName(mName), ".xml"), ignoreList);

    if (fileName.Empty())
      return false;

    bool loadOkay = doc.LoadFile(fileName.c_str());

    //if loading the class file failed, search for a struct file
    if (!loadOkay)
    {
      fileName = FindFile(doxyPath, BuildString("struct_zero_1_1"
        , GetDoxygenName(mName).c_str(), ".xml"), ignoreList);

      if (fileName.Empty())
        return false;

      loadOkay = doc.LoadFile(fileName.c_str());

      if (!loadOkay)
      {
        fileName = FindFile(doxyPath, BuildString("struct_zero_1_1_physics_1_1"
          , GetDoxygenName(mName).c_str(), ".xml"), ignoreList);

        if (fileName.Empty())
          return false;

        loadOkay = doc.LoadFile(fileName.c_str());
      }
    }
    if (!loadOkay)
    {
      WriteLog("Failed to load doc file for %s\n", mName.c_str());
      return false;
    }

    if (!SetRelativePath(doxyPath, fileName))
      return false;

    mHasBeenLoaded = true;
    return true;
  }

  const String& RawClassDoc::GetDescriptionForMethod(StringParam methodName)
  {
    // check if we have a method with a mDescription by that name
    forRange(RawMethodDoc* method, mMethodMap[methodName].All())
    {
      if (method->mDescription.SizeInBytes() > 0)
        return method->mDescription;
    }

    return mMethodMap[methodName].All()[0]->mDescription;
  }

  void RawClassDoc::NormalizeAllTypes(RawTypedefLibrary* defLib)
  {
    forRange(RawVariableDoc* prop, mVariables.All())
    {
      NormalizeTokensFromTypedefs(*prop->mTokens, defLib, mNamespace);
    }
    for (uint i = 0; i < mMethods.Size(); ++i)
    {
      mMethods[i]->NormalizeAllTypes(defLib, mNamespace);
    }
  }

  void RawClassDoc::FillExistingMethodFromDoxygen(StringParam name, TiXmlElement* memberElement, TiXmlNode* memberDef)
  {
    // we don't want to actually save this, just use it to copy data from doxygen
    RawMethodDoc* tempLoadedMethodDoc = new RawMethodDoc(memberElement, memberDef);

    tempLoadedMethodDoc->NormalizeAllTypes(RawTypedefLibrary::Get(), mNamespace);

    // we don't care if we actually found a match, because that just means we don't fill it
    FillMatchingMethod(tempLoadedMethodDoc);

    delete tempLoadedMethodDoc;
  }

  RawMethodDoc* RawClassDoc::FillMatchingMethod(RawMethodDoc* tempDoc)
  {
    // tries to find matching method in this class
    RawMethodDoc* existingMethod = FindMatchingMethod(tempDoc);

    if (!existingMethod)
    {
      String baseClassString = mBaseClass;

      while (!baseClassString.Empty() && mParentLibrary->mClassMap.ContainsKey(mBaseClass))
      {
        RawClassDoc* baseClassDoc = mParentLibrary->mClassMap[baseClassString];

        if (!baseClassDoc)
          break;

        existingMethod = baseClassDoc->FindMatchingMethod(tempDoc);

        if (existingMethod)
          break;

        baseClassString = baseClassDoc->mBaseClass;
      }

      // if a base class had the existing method, make a copy of the method locally
      if (existingMethod)
      {
        existingMethod = new RawMethodDoc(existingMethod);
      }
      else
      {
        return nullptr;
      }
    }

    existingMethod->mDescription = tempDoc->mDescription;
    
    for (uint i = 0; i < existingMethod->mParsedParameters.Size(); ++i)
    {
      existingMethod->mParsedParameters[i]->mName
        = tempDoc->mParsedParameters[i]->mName;
    
      if (existingMethod->mParsedParameters[i]->mDescription.Empty())
      {
        existingMethod->mParsedParameters[i]->mDescription
          = tempDoc->mParsedParameters[i]->mDescription;
      }
    }
    
    return existingMethod;
  }

  Zero::RawMethodDoc* RawClassDoc::FindMatchingMethod(RawMethodDoc* tempDoc)
  {
    // get list of methods with the same name
    Array<RawMethodDoc*>& sameNamedMethods = mMethodMap[tempDoc->mName];

    forRange(RawMethodDoc *existingMethod, sameNamedMethods.All())
    {
      bool matchingMethods = true;

      // try to find matching parameters
      if (existingMethod->mParsedParameters.Size() != tempDoc->mParsedParameters.Size())
      {
        matchingMethods = false;
        continue;
      }
      // since we at least have the right number of parameters, compare types
      for (uint i = 0; i < existingMethod->mParsedParameters.Size(); ++i)
      {
        TypeTokens* existingTokens = existingMethod->mParsedParameters[i]->mTokens;
        TypeTokens* tempLoadedTokens = tempDoc->mParsedParameters[i]->mTokens;

        for (uint j = 0; j < existingTokens->Size(); ++j)
        {

          if ((*existingTokens)[j].mText != (*tempLoadedTokens)[j].mText)
          {
            matchingMethods = false;
            break;
          }
        }

        if (matchingMethods == false)
        {
          break;
        }
      }

      if (matchingMethods)
      {
        return existingMethod;
      }
    }

    return nullptr;
  }

  String RawClassDoc::GenerateMapKey(void)
  {
    StringBuilder builder;

    for (uint i = 0; i < mNamespace.mNames.Size(); ++i)
    {
      builder.Append(mNamespace.mNames[i]);
    }

    builder.Append(mName);

    return builder.ToString();
  }

  ////////////////////////////////////////////////////////////////////////
  // RawMethodDoc
  ////////////////////////////////////////////////////////////////////////
  RawMethodDoc::Parameter::Parameter(void)
  {
    mTokens = new TypeTokens; 
  }

  RawMethodDoc::Parameter::~Parameter(void)
  {
    delete mTokens;
  }

  void RawMethodDoc::Parameter::Serialize(Serializer& stream)
  {
    SerializeNameDefault(mName, String(""));
    SerializeName(mDescription);
    SerializeName(mTokens);
  }

  template<> struct Zero::Serialization::Trait<RawMethodDoc>
  {

    enum { Type = StructureType::Object };
    static inline cstr TypeName() { return "RawMethodDoc"; }
  };

  RawMethodDoc::RawMethodDoc(void)
  { 
    mReturnTokens = new TypeTokens;
    mStatic = false;
  }

  RawMethodDoc::RawMethodDoc(RawMethodDoc *copy)
  {
    forRange(Parameter *oldParam, mParsedParameters.All())
    {
      delete oldParam;
    }

    mReturnTokens = new TypeTokens();

    *mReturnTokens = *(copy->mReturnTokens);

    forRange(Parameter* param, copy->mParsedParameters.All())
    {
      Parameter* paramCopy = new Parameter;

      paramCopy->mTokens = new TypeTokens;

      *(paramCopy->mTokens) = *(param->mTokens);

      paramCopy->mName = param->mName;
      paramCopy->mDescription = param->mDescription;

      mParsedParameters.PushBack(paramCopy);
    }

    mName = copy->mName;
    mDescription = copy->mDescription;
    mStatic = copy->mStatic;
  }

  RawMethodDoc::~RawMethodDoc(void)
  {
    delete mReturnTokens;

    forRange(Parameter* param, mParsedParameters.All())
    {
      delete param;
    }
  }

  RawMethodDoc::RawMethodDoc(TiXmlElement* element, TiXmlNode* currMethod)
  {
    mReturnTokens = new TypeTokens;

    mName = GetElementValue(element, gElementTags[eNAME]);

    mDescription = DoxyToString(element, gElementTags[eBRIEFDESCRIPTION]).Trim();

    mDescription = CleanRedundantSpacesInDesc(mDescription);

    mStatic = false;

    StringBuilder retTypeStr;

    BuildFullTypeString(element, &retTypeStr);

    AppendTokensFromString(DocLangDfa::Get(), retTypeStr.ToString(), mReturnTokens);

    TiXmlNode* firstElement = GetFirstNodeOfChildType(element, gElementTags[ePARAM]);

    TiXmlNode* endNode = GetEndNodeOfChildType(element, gElementTags[ePARAM]);

    // now unpack each parameter seperate
    for (TiXmlNode* param = firstElement; param != endNode; param = param->NextSibling())
    {
      TiXmlElement* paramElement = param->ToElement();

      // create the Parameter doc
      RawMethodDoc::Parameter* parameterDoc = new RawMethodDoc::Parameter;

      // get the name of this arguement
      parameterDoc->mName = GetElementValue(paramElement, gElementTags[eDECLNAME]);

      // get the typenode
      TiXmlNode* typeNode = GetFirstNodeOfChildType(paramElement, gElementTags[eTYPE]);

      if (!typeNode)
        continue;

      // if the typenode existed, get the actual type from its children
      StringBuilder paramName;
      BuildFullTypeString(paramElement, &paramName);

      AppendTokensFromString(DocLangDfa::Get(), paramName.ToString(), parameterDoc->mTokens);

      // get brief description
      TiXmlNode* brief = GetFirstNodeOfChildType(paramElement, gElementTags[eBRIEFDESCRIPTION]);

      if (brief)
      {
        StringBuilder builder;
        getTextFromParaNodes(brief, &builder);
        parameterDoc->mDescription = builder.ToString().Trim();
      }
      else
      {
        // check for inbody
        brief = GetFirstNodeOfChildType(paramElement, "inbodydescription");

        if (brief)
        {
          StringBuilder builder;
          getTextFromParaNodes(brief, &builder);
          parameterDoc->mDescription = builder.ToString().Trim();
        }
      }
      parameterDoc->mDescription = CleanRedundantSpacesInDesc(parameterDoc->mDescription);
      mParsedParameters.PushBack(parameterDoc);
    }
  }


  void RawMethodDoc::LoadFromDoxygen(TiXmlElement* element, TiXmlNode* methodDef)
  {
    mDescription = DoxyToString(element, gElementTags[eBRIEFDESCRIPTION]).Trim();

    mDescription = CleanRedundantSpacesInDesc(mDescription);

    TiXmlNode* firstElement = GetFirstNodeOfChildType(element, gElementTags[ePARAM]);

    TiXmlNode* endNode = GetEndNodeOfChildType(element, gElementTags[ePARAM]);

    // now unpack each parameter separate
    int i = 0;
    for (TiXmlNode* param = firstElement; param != endNode; param = param->NextSibling(), ++i)
    {
      TiXmlElement* paramElement = param->ToElement();

      // get the Parameter doc
      RawMethodDoc::Parameter* parameterDoc = mParsedParameters[i];

      // get the name of this argument
      String parName = GetElementValue(paramElement, gElementTags[eDECLNAME]);

      parameterDoc->mName = parName;

      // get brief description
      TiXmlNode* brief = GetFirstNodeOfChildType(paramElement, gElementTags[eBRIEFDESCRIPTION]);

      if (brief)
      {
        StringBuilder builder;
        getTextFromParaNodes(brief, &builder);
        parameterDoc->mDescription = builder.ToString().Trim();
      }
      else
      {
        // check for inbody
        brief = GetFirstNodeOfChildType(paramElement, "inbodydescription");

        if (brief)
        {
          StringBuilder builder;
          getTextFromParaNodes(brief, &builder);
          parameterDoc->mDescription = builder.ToString().Trim();
        }
      }
      parameterDoc->mDescription = CleanRedundantSpacesInDesc(parameterDoc->mDescription);
    }
  }

  void RawMethodDoc::Serialize(Serializer& stream)
  {
    SerializeName(mName);
    SerializeName(mDescription);
    SerializeNameDefault(mParsedParameters, Array<Parameter* >());
    SerializeName(mReturnTokens);
  }

  void RawMethodDoc::FillTrimmedMethod(MethodDoc* trimMethod)
  {
    TypeTokens* tokens = mReturnTokens;

    trimMethod->mReturnType = TrimTypeTokens(*tokens);

    trimMethod->mName = mName;

    trimMethod->mDescription = mDescription;

    trimMethod->mPossibleExceptionThrows = mPossibleExceptionThrows;

    trimMethod->mStatic = mStatic;

    StringBuilder paramBuilder;
    paramBuilder.Append('(');
    for (uint i = 0; i < mParsedParameters.Size(); ++i)
    {
      RawMethodDoc::Parameter* rawParam = mParsedParameters[i];

      // if we have no parameters (or we have void parameter) just leave loop
      if (rawParam->mTokens->Size() == 0
        || (*rawParam->mTokens)[0].mEnumTokenType == DocTokenType::Void)
      {
        break;
      }

      ParameterDoc* trimParam = new ParameterDoc;

      trimParam->mName = rawParam->mName;

      trimParam->mDescription = rawParam->mDescription;

      tokens = rawParam->mTokens;

      trimParam->mType = TrimTypeTokens(*tokens);

      trimMethod->mParameterList.PushBack(trimParam);

      if (i > 0)
        paramBuilder.Append(", ");

      paramBuilder.Append(trimParam->mType);

      if (trimParam->mName != "")
      {
        paramBuilder.Append(" ");
        paramBuilder.Append(trimParam->mName);
      }
    }
    paramBuilder.Append(')');

    trimMethod->mParameters = paramBuilder.ToString();
  }

  // replaces any typedef'd types found with underlying type
  void RawMethodDoc::NormalizeAllTypes(RawTypedefLibrary* defLib, RawNamespaceDoc& classNamespace)
  {
    // normalize return tokens
    NormalizeTokensFromTypedefs(*mReturnTokens, defLib, classNamespace);

    // normalize parameters
    forRange(Parameter* arg, mParsedParameters.All())
    {
      NormalizeTokensFromTypedefs(*arg->mTokens, defLib, classNamespace);
    }    
  }

  bool RawMethodDoc::MethodHasSameSignature(const RawMethodDoc& methodToCompare,
    bool* retThisHadAnyType, bool* retOtherHadAnyType)
  {
    static const char* anyType = "any";

    // compare param count
    if (mParsedParameters.Size() != methodToCompare.mParsedParameters.Size())
      return false;

    // compare return type (any should match every type)
    if (*mReturnTokens != *methodToCompare.mReturnTokens)
    {
      bool thisHadAnyType  = true;
      bool otherHadAnyType = true;

      if ((*mReturnTokens)[0].mText != anyType)
        thisHadAnyType = false;

      if ((*methodToCompare.mReturnTokens)[0].mText != anyType)
        otherHadAnyType = false;

      if (!thisHadAnyType && !otherHadAnyType)
            return false;

      if (retThisHadAnyType)
        *retThisHadAnyType = thisHadAnyType;

      if (retOtherHadAnyType)
        *retOtherHadAnyType = otherHadAnyType;
    }

    // compare param types (any should match every type)
    for (uint i = 0; i < mParsedParameters.Size(); ++i)
    {
      const Parameter* ourParam = mParsedParameters[i];
      const Parameter* compareParam = mParsedParameters[i];

      if (*ourParam->mTokens != *compareParam->mTokens)
      {
        bool thisHadAnyType = true;
        bool otherHadAnyType = true;

        // was our param type 'any'
        if ((*mReturnTokens)[0].mText != anyType)
          thisHadAnyType = false;

        // was other type 'any'
        if ((*methodToCompare.mReturnTokens)[0].mText != anyType)
          otherHadAnyType = false;

        // if neither were, these methods are not the same, return false
        if (!thisHadAnyType && !otherHadAnyType)
          return false;

        // return information about any, if we were passed bools to store that in
        if (retThisHadAnyType)
          *retThisHadAnyType = thisHadAnyType;

        if (retOtherHadAnyType)
          *retOtherHadAnyType = otherHadAnyType;
      }
    }

    return true;
  }

  ////////////////////////////////////////////////////////////////////////
  // RawTypedefLibrary
  ////////////////////////////////////////////////////////////////////////
  //ZeroDefineType(RawTypedefLibrary);
  ZilchDefineType(RawTypedefLibrary, builder, type)
  {

  }

  template<> struct Zero::Serialization::Trait<RawTypedefLibrary>
  {

    enum { Type = StructureType::Object };
    static inline cstr TypeName() { return "RawTypedefLibrary"; }
  };

  Zero::RawTypedefLibrary* RawTypedefLibrary::Get()
  {
    static RawTypedefLibrary singleton;

    return &singleton;
  }

  void RawTypedefLibrary::LoadTypedefsFromDocLibrary(RawDocumentationLibrary& docLib)
  {
    WriteLog("\nLoading Typedefs from Intermediate Documentation Library\n\n");

    mTypedefs.Clear();
    mTypedefArray.Clear();
    forRange(RawClassDoc* classDoc, docLib.mClasses.All())
    {
      forRange(RawTypedefDoc* tdefDoc, classDoc->mTypedefs.All())
      {
        String mapKey = tdefDoc->GenerateMapKey();
        if (mTypedefs.ContainsKey(mapKey))
        {
          WriteLog("WARNING: Duplicate Typedef: %s\n%*s\nFrom Class: %s\n\n",
            tdefDoc->mType.c_str(), 10, "", classDoc->mName.c_str());
          continue;
        }

        mTypedefArray.PushBack(*tdefDoc);

        mTypedefs[mapKey] = &mTypedefArray.Back();
      }
    }

    Zero::Sort(mTypedefArray.All(), TypedefDocCompareFn);

    BuildMap();

    printf("\n...Done Loading mTypedefs from Intermediate Documentation Library.\n");
  }

  void RawTypedefLibrary::LoadTypedefsFromNamespaceDocumentation(StringParam doxypath)
  {
    WriteLog("Loading Typedefs from Doxygen Namespace XML Files at: %s\n\n", doxypath.c_str());

    Array<String> namespaceFilepaths;

    GetFilesWithPartialName(doxypath, "namespace_", mIgnoreList, &namespaceFilepaths);

    // for each filepath
    for (uint i = 0; i < namespaceFilepaths.Size(); ++i)
    {
      // open the doxy file
      TiXmlDocument doc;
      if (!doc.LoadFile(namespaceFilepaths[i].c_str()))
      {
        // print error on failure to load
        WriteLog("ERROR: unable to load file at: %s\n", namespaceFilepaths[i].c_str());
        continue;
      }

      // get doxygen node
      // get compounddef node
      TiXmlElement* namespaceDef = doc.FirstChildElement(gElementTags[eDOXYGEN])
        ->FirstChildElement(gElementTags[eCOMPOUNDDEF]);

      // we get the compound name so we can get the namespace for this typedef
      TiXmlElement* compoundName = namespaceDef->FirstChildElement("compoundname");

      // get the string out of the compoundName node
      String namespaceName = GetTextFromAllChildrenNodesRecursively(compoundName->FirstChild());

      TypeTokens namespaceTokens;

      AppendTokensFromString(DocLangDfa::Get(), namespaceName, &namespaceTokens);

      TiXmlNode* firstSectDef = GetFirstNodeOfChildType(namespaceDef, gElementTags[eSECTIONDEF]);
      TiXmlNode* endSectDef = GetEndNodeOfChildType(namespaceDef, gElementTags[eSECTIONDEF]);

      TiXmlElement* typedefSection = nullptr;

      // find the typedef section
      for (TiXmlNode* node = firstSectDef; node != endSectDef; node = node->NextSibling())
      {
        TiXmlElement* element = node->ToElement();

        // kind is always first attribute for sections
        TiXmlAttribute* attrib = element->FirstAttribute();

        if (attrib && strcmp(attrib->Value(), "typedef") == 0)
        {
          typedefSection = element;
          break;
        }
      }

      // if this is still null, this namespace doc just has no mTypedefs
      if (typedefSection == nullptr)
        continue;

      TiXmlNode* firstTypedef = GetFirstNodeOfChildType(typedefSection, gElementTags[eMEMBERDEF]);
      TiXmlNode* endTypedef = GetEndNodeOfChildType(typedefSection, gElementTags[eMEMBERDEF]);

      // all memberdefs in this section will be mTypedefs, so just parse the values of each
      for (TiXmlNode* node = firstTypedef; node != endTypedef; node = node->NextSibling())
      {
        // get the name to make sure we don't already have a typedef by this name
        TiXmlNode* nameNode = GetFirstNodeOfChildType(node->ToElement(), gElementTags[eNAME]);

        String mName = GetTextFromAllChildrenNodesRecursively(nameNode);

        RawTypedefDoc* newDoc = &mTypedefArray.PushBack();

        newDoc->LoadFromElement(node->ToElement());

        newDoc->mNamespace.GetNamesFromTokens(namespaceTokens);

        String key = newDoc->GenerateMapKey();

        if (mTypedefs.ContainsKey(key))
        {
          mTypedefArray.PopBack();
          WriteLog("WARNING: Duplicate Typedef Key: %s\n\n", key.c_str());
          continue;
        }

        mTypedefs[key] = newDoc;
      }

    } // end namespace file loop

    Zero::Sort(mTypedefArray.All(), TypedefDocCompareFn);

    BuildMap();

    printf("\n...Done Loading Typedefs from Doxygen Namespace XML Files.\n");
  }

  RawTypedefLibrary::~RawTypedefLibrary(void)
  {
  }

  void RawTypedefLibrary::GenerateTypedefDataFile(const StringParam directory)
  {
    String absPath = BuildString(directory, "\\Typedefs.data");

    WriteLog("writing raw typedef documentation file to location: %s\n\n", absPath.c_str());

    if (!DirectoryExists(directory))
    {
      WriteLog("directory does not exist, creating it now.\n");
      CreateDirectoryAndParents(directory);
    }

    if (!SaveToFile(absPath))
    {
      Error("Failed to write raw typedef documentation\n");
      WriteLog("Failed to write raw typedef documentation\n");
    }
    printf("done writing raw typedef documentation file\n");
  }

  void RawTypedefLibrary::Serialize(Serializer& stream)
  {
    SerializeName(mTypedefArray);
  }

  bool RawTypedefLibrary::SaveToFile(StringParam absPath)
  {
    Status status;

    TextSaver saver;

    saver.Open(status, absPath.c_str());

    if (status.Failed())
    {
      Error(status.Message.c_str());
      return false;
    }

    saver.StartPolymorphic("Doc");

    saver.SerializeField("RawTypedefLibrary", *this);

    saver.EndPolymorphic();

    return true;
  }

  bool RawTypedefLibrary::LoadFromFile(StringParam filepath)
  {    
    Status status;

    DataTreeLoader loader;

    loader.OpenFile(status, filepath.c_str());

    if (status.Failed())
    {
      Error(status.Message.c_str());
      return false;
    }

    PolymorphicNode dummyNode;
    loader.GetPolymorphic(dummyNode);

    loader.SerializeField("Typedefs", mTypedefArray);

    loader.Close();

    // for each typedef replacement, make sure the zilch type is valid and starting with uppercase
    forRange(RawTypedefDoc& typedefDoc, mTypedefArray.All())
    {
      if (!typedefDoc.mDefinition.Empty())
      {
        DocToken& replacementToken = typedefDoc.mDefinition[0];

        // if the replacement was not blank and it started with lowercase
        if (!replacementToken.mText.Empty() 
          && replacementToken.mText.c_str()[0] == replacementToken.mText.ToLower().c_str()[0])
        {
          Assert("Invalid replacement for type: %s, Invalid replacement was: %s"
            , typedefDoc.mType.c_str(), replacementToken.mText.c_str());
        }
      }
    }

    // just to be safe, sort the serialized array (even though it already should be)
    Zero::Sort(mTypedefArray.All(), TypedefDocCompareFn);

    BuildMap();

    return true;
  }

  void RawTypedefLibrary::BuildMap(void)
  {
    mTypedefs.Clear();

    for (uint i = 0; i < mTypedefArray.Size(); ++i)
    {
      RawTypedefDoc* tdoc = &mTypedefArray[i];

      mTypedefs[tdoc->GenerateMapKey()] = tdoc;
    }
  }

  // this is literally just the Normalize Tokens function with an extra param for typedefs
  bool NormalizeTypedefWithTypedefs(StringParam typedefKey, TypeTokens& tokens,
    RawTypedefLibrary* defLib, RawNamespaceDoc& classNamespace)
  {
    bool madeReplacements = false;

    // loop over tokens
    for (uint i = 0; i < tokens.Size(); ++i)
    {
      //should be moved into the loop
      Array<String>& names = classNamespace.mNames;

      DocToken& token = tokens[i];

      // check any namespaces this token list is inside

      String key = token.mText;

      // loop over possible namespace'd version
      for (int j = -1; j < (int)names.Size(); ++j)
      {
        StringBuilder builder;

        for (uint k = 0; (int)k <= j; ++k)
          builder.Append(names[k]);

        builder.Append(token.mText);

        key = builder.ToString();

        if (defLib->mTypedefs.ContainsKey(key))
        {
          TypeTokens* typedefTokens = &defLib->mTypedefs[key]->mDefinition;

          // make sure they are not just the same tokens
          if (typedefKey == key || (tokens.Size() >= typedefTokens->Size()
            && ContainsFirstTypeInSecondType(*typedefTokens, tokens)))
          {
            break;
          }


          // this next chunk of crazy to make sure we don't redundantly expand any typedefs
          // first we have to make sure our token ranges are of valid size
          if ((int)i - 2 > 0 && typedefTokens->Size() >= 3)
          {
            bool equal = true;

            // the magic number 3 is because we are checking for this: typedef name ns::name
            for (uint m = 0; m < 3; ++m)
            {
              if ((*typedefTokens)[m].mText != tokens.SubRange(i - 2, 3)[m].mText)
                equal = false;
            }
            if (equal)
            {
              break;
            }
          }

          madeReplacements = true;
          i += ReplaceTypedefAtLocation(tokens, &token, *defLib->mTypedefs[key]);
          return madeReplacements;
        }
      }

    }
    return madeReplacements;
  }

  void RawTypedefLibrary::ExpandAllTypedefs(void)
  {
    typedef Pair<String, RawTypedefDoc* > mapIndex;
    BuildMap();

    // flag to see if we found a replacement
    bool replacements = false;

    Array<String> replacementList;

    // for each typedef
    for(mapIndex *iter = mTypedefs.Begin(); iter != mTypedefs.End(); ++iter)
    {
      RawTypedefDoc* tDef = iter->second;

      if (NormalizeTypedefWithTypedefs(iter->first,tDef->mDefinition, this, tDef->mNamespace))
      {
        replacements = true;
        replacementList.Append(iter->first);
      }
    }
    
    // while we have found a replacement
    while (replacements)
    {
      Array<String> newReplacementList;
    
      replacements = false;
      for (uint i = 0; i < replacementList.Size(); ++i)
      {
        String& name = replacementList[i];
        RawTypedefDoc *tDef = mTypedefs[name];
    
        if (NormalizeTypedefWithTypedefs(name,tDef->mDefinition, this, tDef->mNamespace))
        {
          replacements = true;
          newReplacementList.Append(name);
        }
    
      }
    
      replacementList.Clear();
      replacementList = newReplacementList;
    }// end of while
    
  }

}
