#include"FarEditorSet.h"

DWORD FarEditorSet::rGetValueDw(size_t Root, const wchar_t *name, DWORD DefaultValue)
{
  FarSettingsItem fsi;
  fsi.Root = Root;
  fsi.Type = FST_QWORD;
  fsi.Name = name;

  if (Info.SettingsControl(farSettingHandle, SCTL_GET, NULL, (INT_PTR)&fsi)){
    return fsi.Number;
  }
  else{
    return DefaultValue;
  }
}

const wchar_t *FarEditorSet::rGetValueSz(size_t Root, const wchar_t *name, const wchar_t *DefaultValue)
{
  FarSettingsItem fsi;
  fsi.Root = Root;
  fsi.Type = FST_STRING;
  fsi.Name = name;

  if (Info.SettingsControl(farSettingHandle, SCTL_GET, NULL, (INT_PTR)&fsi)){
    return fsi.String;
  }
  else{
    return DefaultValue;
  }

}

bool FarEditorSet::rSetValueDw(size_t Root, const wchar_t *name, DWORD val)
{
  FarSettingsItem fsi;
  fsi.Root = Root;
  fsi.Type = FST_QWORD;
  fsi.Name = name;
  fsi.Number = val;

  return !!Info.SettingsControl(farSettingHandle, SCTL_SET, NULL, (INT_PTR)&fsi);

}

bool FarEditorSet::rSetValueSz(size_t Root, const wchar_t *name, const wchar_t *val)
{
  FarSettingsItem fsi;
  fsi.Root = Root;
  fsi.Type = FST_STRING;
  fsi.Name = name;
  fsi.String = val;

  return !!Info.SettingsControl(farSettingHandle, SCTL_SET, NULL, (INT_PTR)&fsi);

}

FarEditorSet::FarEditorSet()
{
  
  FarSettingsCreate fsc;
  fsc.Guid = MainGuid;
  fsc.StructSize = sizeof(FarSettingsCreate);
  if (Info.SettingsControl(INVALID_HANDLE_VALUE, SCTL_CREATE, NULL, (INT_PTR)&fsc)){
    farSettingHandle = fsc.Handle;
  }
  else{
    //error
  }

  rEnabled = !!rGetValueDw(0, cRegEnabled, cEnabledDefault);
  parserFactory = NULL;
  regionMapper = NULL;
  hrcParser = NULL;
  sHrdName = NULL;
  sHrdNameTm = NULL;
  sCatalogPath = NULL;
  sCatalogPathExp = NULL;
  sTempHrdName = NULL;
  sTempHrdNameTm = NULL;
  sUserHrdPath = NULL;
  sUserHrdPathExp = NULL;
  sUserHrcPath = NULL;
  sUserHrcPathExp = NULL;

  ReloadBase();
  viewFirst = 0;
}

FarEditorSet::~FarEditorSet()
{
  dropAllEditors(false);
  Info.SettingsControl(farSettingHandle, SCTL_FREE, NULL, NULL);
  delete sHrdName;
  delete sHrdNameTm;
  delete sCatalogPath;
  delete sUserHrdPath;
  delete sUserHrdPathExp;
  delete sUserHrcPath;
  delete sUserHrcPathExp;
  delete regionMapper;
  delete parserFactory;
}


void FarEditorSet::openMenu()
{
  int iMenuItems[] =
  {
    mListTypes, mMatchPair, mSelectBlock, mSelectPair,
    mListFunctions, mFindErrors, mSelectRegion, mLocateFunction, -1,
    mUpdateHighlight, mReloadBase, mConfigure
  };
  FarMenuItem menuElements[sizeof(iMenuItems) / sizeof(iMenuItems[0])];
  memset(menuElements, 0, sizeof(menuElements));

  try{
    if (!rEnabled){
      menuElements[0].Text = GetMsg(mConfigure);
      menuElements[0].Flags = MIF_SELECTED;

      if (Info.Menu(&MainGuid, -1, -1, 0, FMENU_WRAPMODE, GetMsg(mName), 0, L"menu", NULL, NULL, menuElements, 1) == 0){
        ReadSettings();
        configure(true);
      }

      return;
    };

    for (int i = sizeof(iMenuItems) / sizeof(iMenuItems[0]) - 1; i >= 0; i--){
      if (iMenuItems[i] == -1){
        menuElements[i].Flags = MIF_SEPARATOR;;
      }
      else{
        menuElements[i].Text = GetMsg(iMenuItems[i]);
      }
    };

    menuElements[0].Flags = MIF_SELECTED;

    // �.�. ������������ ������� getCurrentEditor ����� ������� NULL, �� ����� 
    // ��������� �� ���. �� �������� �������� NULL �� ���������, ������ ��� �� � ������ �����
    FarEditor *editor = getCurrentEditor();
    switch (Info.Menu(&MainGuid, -1, -1, 0, FMENU_WRAPMODE, GetMsg(mName), 0, L"menu", NULL, NULL,
      menuElements, sizeof(iMenuItems) / sizeof(iMenuItems[0])))
    {
    case 0:
      if (editor){
        chooseType();
      }
      break;
    case 1:
      if (editor){
        editor->matchPair();
      }
      break;
    case 2:
      if (editor){
        editor->selectBlock();
      }
      break;
    case 3:
      if (editor){
        editor->selectPair();
      }
      break;
    case 4:
      if (editor){
        editor->listFunctions();
      }
      break;
    case 5:
      if (editor){
        editor->listErrors();
      }
      break;
    case 6:
      if (editor){
        editor->selectRegion();
      }
      break;
    case 7:
      if (editor){
        editor->locateFunction();
      }
      break;
    case 9:
      if (editor){
        editor->updateHighlighting();
      }
      break;
    case 10:
      ReloadBase();
      break;
    case 11:
      configure(true);
      break;
    };
  }
  catch (Exception &e){
    const wchar_t* exceptionMessage[5];
    exceptionMessage[0] = GetMsg(mName);
    exceptionMessage[1] = GetMsg(mCantLoad);
    exceptionMessage[3] = GetMsg(mDie);
    StringBuffer msg("openMenu: ");
    exceptionMessage[2] = (msg+e.getMessage()).getWChars();

    if (getErrorHandler()){
      getErrorHandler()->error(*e.getMessage());
    }

    Info.Message(&MainGuid, FMSG_WARNING, L"exception", &exceptionMessage[0], 4, 1);
    disableColorer();
  };
}


void FarEditorSet::viewFile(const String &path)
{
  if (viewFirst==0) viewFirst=1;
  try{
    if (!rEnabled){
      throw Exception(DString("Colorer is disabled"));
    }

    // Creates store of text lines
    TextLinesStore textLinesStore;
    textLinesStore.loadFile(&path, NULL, true);
    // Base editor to make primary parse
    BaseEditor baseEditor(parserFactory, &textLinesStore);
    RegionMapper *regionMap;
    try{
      regionMap=parserFactory->createStyledMapper(&DConsole, sHrdName);
    }
    catch (ParserFactoryException &e){
      if (getErrorHandler() != NULL){
        getErrorHandler()->error(*e.getMessage());
      }
      regionMap = parserFactory->createStyledMapper(&DConsole, NULL);
    };
    baseEditor.setRegionMapper(regionMap);
    baseEditor.chooseFileType(&path);
    // initial event
    baseEditor.lineCountEvent(textLinesStore.getLineCount());
    // computing background color
    int background = 0x1F;
    const StyledRegion *rd = StyledRegion::cast(regionMap->getRegionDefine(DString("def:Text")));

    if (rd != NULL && rd->bfore && rd->bback){
      background = rd->fore + (rd->back<<4);
    }

    // File viewing in console window
    TextConsoleViewer viewer(&baseEditor, &textLinesStore, background, -1);
    viewer.view();
    delete regionMap;
  }
  catch (Exception &e){
    const wchar_t* exceptionMessage[4];
    exceptionMessage[0] = GetMsg(mName);
    exceptionMessage[1] = GetMsg(mCantOpenFile);
    exceptionMessage[3] = GetMsg(mDie);
    exceptionMessage[2] = e.getMessage()->getWChars();
    Info.Message(&MainGuid, FMSG_WARNING, L"exception", &exceptionMessage[0], 4, 1);
  };
}

int FarEditorSet::getCountFileTypeAndGroup()
{
  int num = 0;
  const String *group = NULL;
  FileType *type = NULL;

  for (int idx = 0;; idx++, num++){
    type = hrcParser->enumerateFileTypes(idx);

    if (type == NULL){
      break;
    }

    if (group != NULL && !group->equals(type->getGroup())){
      num++;
    }

    group = type->getGroup();
  };
  return num;
}

FileTypeImpl* FarEditorSet::getFileTypeByIndex(int idx)
{
  FileType *type = NULL;
  const String *group = NULL;

  for (int i = 0; idx>=0; idx--, i++){
    type = hrcParser->enumerateFileTypes(i);

    if (!type){
      break;
    }

    if (group != NULL && !group->equals(type->getGroup())){
      idx--;
    }
    group = type->getGroup();
  };

  return (FileTypeImpl*)type;
}

void FarEditorSet::chooseType()
{
  FarEditor *fe = getCurrentEditor();
  if (!fe){
    return;
  }

  int num = getCountFileTypeAndGroup();
  const String *group = NULL;
  FileType *type = NULL;

  char MapThis[] = "1234567890QWERTYUIOPASDFGHJKLZXCVBNM";
  FarMenuItem *menuels = new FarMenuItem[num];
  memset(menuels, 0, sizeof(FarMenuItem)*(num));
  group = NULL;

  for (int idx = 0, i = 0;; idx++, i++){
    type = hrcParser->enumerateFileTypes(idx);

    if (type == NULL){
      break;
    }

    if (group != NULL && !group->equals(type->getGroup())){
      menuels[i].Flags = MIF_SEPARATOR;
      i++;
    };

    group = type->getGroup();

    const wchar_t *groupChars = NULL;

    if (group != NULL){
      groupChars = group->getWChars();
    }
    else{
      groupChars = L"<no group>";
    }

    menuels[i].Text = new wchar_t[255];
    _snwprintf((wchar_t*)menuels[i].Text, 255, L"%c. %s: %s", idx < 36?MapThis[idx]:'x', groupChars, type->getDescription()->getWChars());

    if (type == fe->getFileType()){
      menuels[i].Flags = MIF_SELECTED;
    }
  };

  wchar_t bottom[20];
  int i;
  _snwprintf(bottom, 20, GetMsg(mTotalTypes), hrcParser->getFileTypesCount());
  i = Info.Menu(&MainGuid, -1, -1, 0, FMENU_WRAPMODE | FMENU_AUTOHIGHLIGHT,
    GetMsg(mSelectSyntax), bottom, L"contents", NULL, NULL, menuels, num);

  for (int idx = 0; idx < num; idx++){
    if (menuels[idx].Text){
      delete[] menuels[idx].Text;
    }
  }

  delete[] menuels;

  if (i == -1){
    return;
  }

 type = getFileTypeByIndex(i);
 if (type != NULL){
    fe->setFileType(type);
  }
}

const String *FarEditorSet::getHRDescription(const String &name, DString _hrdClass )
{
  const String *descr = NULL;
  if (parserFactory != NULL){
    descr = parserFactory->getHRDescription(_hrdClass, name);
  }

  if (descr == NULL){
    descr = &name;
  }

  return descr;
}

INT_PTR WINAPI SettingDialogProc(HANDLE hDlg, int Msg, int Param1, INT_PTR Param2) 
{
  FarEditorSet *fes = (FarEditorSet *)Info.SendDlgMessage(hDlg,DM_GETDLGDATA,0,0);; 

  switch (Msg){
  case DN_BTNCLICK:
    switch (Param1){
  case IDX_HRD_SELECT:
    {
      SString *tempSS = new SString(fes->chooseHRDName(fes->sTempHrdName, DConsole));
      delete fes->sTempHrdName;
      fes->sTempHrdName=tempSS;
      const String *descr=fes->getHRDescription(*fes->sTempHrdName,DConsole);
      Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,IDX_HRD_SELECT,(LONG_PTR)descr->getWChars());
      return true;
    }
    break;
  case IDX_HRD_SELECT_TM:
    {
      SString *tempSS = new SString(fes->chooseHRDName(fes->sTempHrdNameTm, DRgb));
      delete fes->sTempHrdNameTm;
      fes->sTempHrdNameTm=tempSS;
      const String *descr=fes->getHRDescription(*fes->sTempHrdNameTm,DRgb);
      Info.SendDlgMessage(hDlg,DM_SETTEXTPTR,IDX_HRD_SELECT_TM,(LONG_PTR)descr->getWChars());
      return true;
    }
    break;
  case IDX_RELOAD_ALL:
    {
      Info.SendDlgMessage(hDlg,DM_SHOWDIALOG , false,0);
      wchar_t *catalog = trim((wchar_t*)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,IDX_CATALOG_EDIT,0));
      wchar_t *userhrd = trim((wchar_t*)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,IDX_USERHRD_EDIT,0));
      wchar_t *userhrc = trim((wchar_t*)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,IDX_USERHRC_EDIT,0));
      fes->TestLoadBase(catalog, userhrd, userhrc, true);
      Info.SendDlgMessage(hDlg,DM_SHOWDIALOG , true,0);
      return true;
    }
    break;
  case IDX_HRC_SETTING:
    {
      fes->configureHrc();
      return true;
    }
    break;
  case IDX_OK:
    const wchar_t *temp = (const wchar_t*)trim((wchar_t*)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,IDX_CATALOG_EDIT,0));
    const wchar_t *userhrd = (const wchar_t*)trim((wchar_t*)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,IDX_USERHRD_EDIT,0));
    const wchar_t *userhrc = (const wchar_t*)trim((wchar_t*)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,IDX_USERHRC_EDIT,0));
    int k = (int)Info.SendDlgMessage(hDlg, DM_GETCHECK, IDX_ENABLED, 0);

    if (fes->GetCatalogPath()->compareTo(DString(temp))|| fes->GetUserHrdPath()->compareTo(DString(userhrd)) || (!fes->GetPluginStatus() && k)){ 
      if (fes->TestLoadBase(temp, userhrd, userhrc, false)){
        return false;
      }
      else{
        return true;
      }
    }

    return false;
    break;
    }
  }

  return Info.DefDlgProc(hDlg, Msg, Param1, Param2);
}

void FarEditorSet::configure(bool fromEditor)
{ 
  try{
    FarDialogItem fdi[] =
    {// type, x1, y1, x2, y2, param, history, mask, flags, userdata, ptrdata, maxlen
      { DI_DOUBLEBOX,3,1,55,22,0,0,0,0,0,L"",0},                //IDX_BOX,
      { DI_CHECKBOX,5,2,0,0,0,0,0,0,0,L"",0},                   //IDX_DISABLED,
      { DI_CHECKBOX,5,3,0,0,0,0,0,DIF_3STATE,0,L"",0},          //IDX_CROSS,
      { DI_CHECKBOX,5,4,0,0,0,0,0,0,0,L"",0},                   //IDX_PAIRS,
      { DI_CHECKBOX,5,5,0,0,0,0,0,0,0,L"",0},                   //IDX_SYNTAX,
      { DI_CHECKBOX,5,6,0,0,0,0,0,0,0,L"",0},                   //IDX_OLDOUTLINE,
      { DI_CHECKBOX,5,7,0,0,0,0,0,0,0,L"",0},                   //IDX_CHANGE_BG,
      { DI_TEXT,5,8,0,8,0,0,0,0,0,L"",0},                       //IDX_HRD,
      { DI_BUTTON,20,8,0,0,0,0,0,0,0,L"",0},                    //IDX_HRD_SELECT,
      { DI_TEXT,5,9,0,9,0,0,0,0,0,L"",0},                       //IDX_CATALOG,
      { DI_EDIT,6,10,52,5,0,L"catalog",0,DIF_HISTORY,0,L"",0},  //IDX_CATALOG_EDIT
      { DI_TEXT,5,11,0,11,0,0,0,0,0,L"",0},                     //IDX_USERHRC,
      { DI_EDIT,6,12,52,5,0,L"userhrc",0,DIF_HISTORY,0,L"",0},  //IDX_USERHRC_EDIT
      { DI_TEXT,5,13,0,13,0,0,0,0,0,L"",0},                     //IDX_USERHRD,
      { DI_EDIT,6,14,52,5,0,L"userhrd",0,DIF_HISTORY,0,L"",0},  //IDX_USERHRD_EDIT
      { DI_SINGLEBOX,4,16,54,16,0,0,0,0,0,L"",0},               //IDX_TM_BOX,
      { DI_CHECKBOX,5,17,0,0,0,0,0,0,0,L"",0},                  //IDX_TRUEMOD,
      { DI_TEXT,20,17,0,17,0,0,0,0,0,L"",0},                    //IDX_TMMESSAGE,
      { DI_TEXT,5,18,0,18,0,0,0,0,0,L"",0},                     //IDX_HRD_TM,
      { DI_BUTTON,20,18,0,0,0,0,0,0,0,L"",0},                   //IDX_HRD_SELECT_TM,
      { DI_SINGLEBOX,4,19,54,19,0,0,0,0,0,L"",0},               //IDX_TM_BOX_OFF,
      { DI_BUTTON,5,20,0,0,0,0,0,0,0,L"",0},                    //IDX_RELOAD_ALL,
      { DI_BUTTON,30,20,0,0,0,0,0,0,0,L"",0},                   //IDX_HRC_SETTING,
      { DI_BUTTON,35,21,0,0,0,0,0,DIF_DEFAULTBUTTON,0,L"",0},   //IDX_OK,
      { DI_BUTTON,45,21,0,0,0,0,0,0,0,L"",0},                   //IDX_CANCEL,
    };// type, x1, y1, x2, y2, param, history, mask, flags, userdata, ptrdata, maxlen

    fdi[IDX_BOX].PtrData = GetMsg(mSetup);
    fdi[IDX_ENABLED].PtrData = GetMsg(mTurnOff);
    fdi[IDX_ENABLED].Selected = rEnabled;
    fdi[IDX_TRUEMOD].PtrData = GetMsg(mTrueMod);
    fdi[IDX_TRUEMOD].Selected = TrueModOn;
    fdi[IDX_CROSS].PtrData = GetMsg(mCross);
    fdi[IDX_CROSS].Selected = drawCross;
    fdi[IDX_PAIRS].PtrData = GetMsg(mPairs);
    fdi[IDX_PAIRS].Selected = drawPairs;
    fdi[IDX_SYNTAX].PtrData = GetMsg(mSyntax);
    fdi[IDX_SYNTAX].Selected = drawSyntax;
    fdi[IDX_OLDOUTLINE].PtrData = GetMsg(mOldOutline);
    fdi[IDX_OLDOUTLINE].Selected = oldOutline;
    fdi[IDX_CATALOG].PtrData = GetMsg(mCatalogFile);
    fdi[IDX_CATALOG_EDIT].PtrData = sCatalogPath->getWChars();
    fdi[IDX_USERHRC].PtrData = GetMsg(mUserHrcFile);
    fdi[IDX_USERHRC_EDIT].PtrData = sUserHrcPath->getWChars();
    fdi[IDX_USERHRD].PtrData = GetMsg(mUserHrdFile);
    fdi[IDX_USERHRD_EDIT].PtrData = sUserHrdPath->getWChars();
    fdi[IDX_HRD].PtrData = GetMsg(mHRDName);

    const String *descr = NULL;
    sTempHrdName =new SString(sHrdName); 
    descr=getHRDescription(*sTempHrdName,DConsole);

    fdi[IDX_HRD_SELECT].PtrData = descr->getWChars();

    const String *descr2 = NULL;
    sTempHrdNameTm =new SString(sHrdNameTm); 
    descr2=getHRDescription(*sTempHrdNameTm,DRgb);

    fdi[IDX_HRD_TM].PtrData = GetMsg(mHRDNameTrueMod);
    fdi[IDX_HRD_SELECT_TM].PtrData = descr2->getWChars();
    fdi[IDX_CHANGE_BG].PtrData = GetMsg(mChangeBackgroundEditor);
    fdi[IDX_CHANGE_BG].Selected = ChangeBgEditor;
    fdi[IDX_RELOAD_ALL].PtrData = GetMsg(mReloadAll);
    fdi[IDX_HRC_SETTING].PtrData = GetMsg(mUserHrcSetting);
    fdi[IDX_OK].PtrData = GetMsg(mOk);
    fdi[IDX_CANCEL].PtrData = GetMsg(mCancel);
    fdi[IDX_TM_BOX].PtrData = GetMsg(mTrueModSetting);

    if (!checkConsoleAnnotationAvailable() && fromEditor){
      fdi[IDX_HRD_SELECT_TM].Flags = DIF_DISABLE;
      fdi[IDX_TRUEMOD].Flags = DIF_DISABLE;
      if (!checkFarTrueMod()){
        if (!checkConEmu()){
          fdi[IDX_TMMESSAGE].PtrData = GetMsg(mNoFarTMConEmu);
        }
        else{
          fdi[IDX_TMMESSAGE].PtrData = GetMsg(mNoFarTM);
        }
      }
      else{
        if (!checkConEmu()){
          fdi[IDX_TMMESSAGE].PtrData = GetMsg(mNoConEmu);
        }
      }
    }
    /*
    * Dialog activation
    */
    HANDLE hDlg = Info.DialogInit(&MainGuid, &PluginConfig, -1, -1, 58, 24, L"config", fdi, ARRAY_SIZE(fdi), 0, 0, SettingDialogProc, (LONG_PTR)this);
    int i = Info.DialogRun(hDlg);

    if (i == IDX_OK){
      fdi[IDX_CATALOG_EDIT].PtrData = (const wchar_t*)trim((wchar_t*)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,IDX_CATALOG_EDIT,0));
      fdi[IDX_USERHRD_EDIT].PtrData = (const wchar_t*)trim((wchar_t*)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,IDX_USERHRD_EDIT,0));
      fdi[IDX_USERHRC_EDIT].PtrData = (const wchar_t*)trim((wchar_t*)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,IDX_USERHRC_EDIT,0));
      //check whether or not to reload the base
      int k = false;

      if (sCatalogPath->compareTo(DString(fdi[IDX_CATALOG_EDIT].PtrData)) || 
          sUserHrdPath->compareTo(DString(fdi[IDX_USERHRD_EDIT].PtrData)) || 
          sUserHrcPath->compareTo(DString(fdi[IDX_USERHRC_EDIT].PtrData)) || 
          sHrdName->compareTo(*sTempHrdName) ||
          sHrdNameTm->compareTo(*sTempHrdNameTm))
      { 
        k = true;
      }

      fdi[IDX_ENABLED].Selected = (int)Info.SendDlgMessage(hDlg, DM_GETCHECK, IDX_ENABLED, 0);
      drawCross = (int)Info.SendDlgMessage(hDlg, DM_GETCHECK, IDX_CROSS, 0);
      drawPairs = !!Info.SendDlgMessage(hDlg, DM_GETCHECK, IDX_PAIRS, 0);
      drawSyntax = !!Info.SendDlgMessage(hDlg, DM_GETCHECK, IDX_SYNTAX, 0);
      oldOutline = !!Info.SendDlgMessage(hDlg, DM_GETCHECK, IDX_OLDOUTLINE, 0);
      ChangeBgEditor = !!Info.SendDlgMessage(hDlg, DM_GETCHECK, IDX_CHANGE_BG, 0);
      fdi[IDX_TRUEMOD].Selected = !!Info.SendDlgMessage(hDlg, DM_GETCHECK, IDX_TRUEMOD, 0);
      delete sHrdName;
      delete sHrdNameTm;
      delete sCatalogPath;
      delete sUserHrdPath;
      delete sUserHrcPath;
      sHrdName = sTempHrdName;
      sHrdNameTm = sTempHrdNameTm;
      sCatalogPath = new SString(DString(fdi[IDX_CATALOG_EDIT].PtrData));
      sUserHrdPath = new SString(DString(fdi[IDX_USERHRD_EDIT].PtrData));
      sUserHrcPath = new SString(DString(fdi[IDX_USERHRC_EDIT].PtrData));

      // if the plugin has been enable, and we will disable
      if (rEnabled && !fdi[IDX_ENABLED].Selected){
        rEnabled = false;
        TrueModOn = !!(fdi[IDX_TRUEMOD].Selected);
        SaveSettings();
        disableColorer();
      }
      else{
        if ((!rEnabled && fdi[IDX_ENABLED].Selected) || k){
          rEnabled = true;
          TrueModOn = !!(fdi[IDX_TRUEMOD].Selected);
          SaveSettings();
          enableColorer(fromEditor);
        }
        else{
          if (TrueModOn !=!!fdi[IDX_TRUEMOD].Selected){
            TrueModOn = !!(fdi[IDX_TRUEMOD].Selected);
            SaveSettings();
            ReloadBase();
          }
          else{
            SaveSettings();
            ApplySettingsToEditors();
            SetBgEditor();
          }
        }
      }
    }

    Info.DialogFree(hDlg);

  }
  catch (Exception &e){
    const wchar_t* exceptionMessage[5];
    exceptionMessage[0] = GetMsg(mName);
    exceptionMessage[1] = GetMsg(mCantLoad);
    exceptionMessage[2] = 0;
    exceptionMessage[3] = GetMsg(mDie);
    StringBuffer msg("configure: ");
    exceptionMessage[2] = (msg+e.getMessage()).getWChars();

    if (getErrorHandler() != NULL){
      getErrorHandler()->error(*e.getMessage());
    }

    Info.Message(&MainGuid, FMSG_WARNING, L"exception", &exceptionMessage[0], 4, 1);
    disableColorer();
  };
}

const String *FarEditorSet::chooseHRDName(const String *current, DString _hrdClass )
{ 
  if (parserFactory == NULL){
    return current;
  }

  int count = parserFactory->countHRD(_hrdClass);
  FarMenuItem *menuElements = new FarMenuItem[count];
  memset(menuElements, 0, sizeof(FarMenuItem)*count);

  for (int i = 0; i < count; i++){
    const String *name = parserFactory->enumerateHRDInstances(_hrdClass, i);
    const String *descr = parserFactory->getHRDescription(_hrdClass, *name);

    if (descr == NULL){
      descr = name;
    }

    menuElements[i].Text = descr->getWChars();

    if (current->equals(name)){
      menuElements[i].Flags = MIF_SELECTED;
    }
  };

  int result = Info.Menu(&MainGuid, -1, -1, 0, FMENU_WRAPMODE|FMENU_AUTOHIGHLIGHT,
    GetMsg(mSelectHRD), 0, L"hrd", NULL, NULL, menuElements, count);
  delete[] menuElements;

  if (result == -1){
    return current;
  }

  return parserFactory->enumerateHRDInstances(_hrdClass, result);
}

int FarEditorSet::editorInput(const INPUT_RECORD *ir)
{
  if (rEnabled){
    FarEditor *editor = getCurrentEditor();
    if (editor){
      return editor->editorInput(ir);
    }
  }
  return 0;
}

int FarEditorSet::editorEvent(int Event, void *Param)
{
  // check whether all the editors cleaned
  if (!rEnabled && farEditorInstances.size() && Event==EE_GOTFOCUS){
    dropCurrentEditor(true);
    return 0;
  }

  if (!rEnabled){
    return 0;
  }

  try{
    FarEditor *editor = NULL;
    switch (Event){
    case EE_REDRAW:
      {
        editor = getCurrentEditor();
        if (editor){
          return editor->editorEvent(Event, Param);
        }
        else{
          return 0;
        }
      }
      break;
    case EE_GOTFOCUS:
      {
        if (!getCurrentEditor()){
          editor = addCurrentEditor();
          return editor->editorEvent(EE_REDRAW, EEREDRAW_CHANGE);
        }
        return 0;
      }
      break;
    case EE_READ:
      {
        addCurrentEditor();
        return 0;
      }
      break;
    case EE_CLOSE:
      {
        editor = farEditorInstances.get(&SString(*((int*)Param)));
        farEditorInstances.remove(&SString(*((int*)Param)));
        delete editor;
        return 0;
      }
      break;
    }
  }
  catch (Exception &e){
    const wchar_t* exceptionMessage[5];
    exceptionMessage[0] = GetMsg(mName);
    exceptionMessage[1] = GetMsg(mCantLoad);
    exceptionMessage[2] = 0;
    exceptionMessage[3] = GetMsg(mDie);
    StringBuffer msg("editorEvent: ");
    exceptionMessage[2] = (msg+e.getMessage()).getWChars();

    if (getErrorHandler()){
      getErrorHandler()->error(*e.getMessage());
    }

    Info.Message(&MainGuid, FMSG_WARNING, L"exception", &exceptionMessage[0], 4, 1);
    disableColorer();
  };

  return 0;
}

bool FarEditorSet::TestLoadBase(const wchar_t *catalogPath, const wchar_t *userHrdPath, const wchar_t *userHrcPath, const int full)
{
  bool res = true;
  const wchar_t *marr[2] = { GetMsg(mName), GetMsg(mReloading) };
  HANDLE scr = Info.SaveScreen(0, 0, -1, -1);
  Info.Message(&MainGuid, 0, NULL, &marr[0], 2, 0);

  ParserFactory *parserFactoryLocal = NULL;
  RegionMapper *regionMapperLocal = NULL;
  HRCParser *hrcParserLocal = NULL;

  SString *catalogPathS = PathToFullS(catalogPath,false);
  SString *userHrdPathS = PathToFullS(userHrdPath,false);
  SString *userHrcPathS = PathToFullS(userHrcPath,false);

  SString *tpath;
  if (!catalogPathS || !catalogPathS->length()){
    StringBuffer *path=new StringBuffer(PluginPath);
    path->append(DString(FarCatalogXml));
    tpath = path;
  }
  else{
    tpath=catalogPathS;
  }

  try{
    parserFactoryLocal = new ParserFactory(tpath);
    delete tpath;
    hrcParserLocal = parserFactoryLocal->getHRCParser();
    LoadUserHrd(userHrdPathS, parserFactoryLocal);
    LoadUserHrc(userHrcPathS, parserFactoryLocal);
    FarHrcSettings p(parserFactoryLocal);
   // p.readProfile();
    //p.readUserProfile();

    try{
      regionMapperLocal = parserFactoryLocal->createStyledMapper(&DConsole, sTempHrdName);
    }
    catch (ParserFactoryException &e)
    {
      if ((parserFactoryLocal != NULL)&&(parserFactoryLocal->getErrorHandler()!=NULL)){
        parserFactoryLocal->getErrorHandler()->error(*e.getMessage());
      }
      regionMapperLocal = parserFactoryLocal->createStyledMapper(&DConsole, NULL);
    };

    delete regionMapperLocal;
    regionMapperLocal=NULL;
    try{
      regionMapperLocal = parserFactoryLocal->createStyledMapper(&DRgb, sTempHrdNameTm);
    }
    catch (ParserFactoryException &e)
    {
      if ((parserFactoryLocal != NULL)&&(parserFactoryLocal->getErrorHandler()!=NULL)){
        parserFactoryLocal->getErrorHandler()->error(*e.getMessage());
      }
      regionMapperLocal = parserFactoryLocal->createStyledMapper(&DRgb, NULL);
    };

    Info.RestoreScreen(scr);
    if (full){
      for (int idx = 0;; idx++){
        FileType *type = hrcParserLocal->enumerateFileTypes(idx);

        if (type == NULL){
          break;
        }

        StringBuffer tname;

        if (type->getGroup() != NULL){
          tname.append(type->getGroup());
          tname.append(DString(": "));
        }

        tname.append(type->getDescription());
        marr[1] = tname.getWChars();
        scr = Info.SaveScreen(0, 0, -1, -1);
        Info.Message(&MainGuid, 0, NULL, &marr[0], 2, 0);
        type->getBaseScheme();
        Info.RestoreScreen(scr);
      }
    }
  }
  catch (Exception &e){
    const wchar_t *errload[5] = { GetMsg(mName), GetMsg(mCantLoad), 0, GetMsg(mFatal), GetMsg(mDie) };

    errload[2] = e.getMessage()->getWChars();

    if ((parserFactoryLocal != NULL)&&(parserFactoryLocal->getErrorHandler()!=NULL)){
      parserFactoryLocal->getErrorHandler()->error(*e.getMessage());
    }

	Info.Message(&MainGuid, FMSG_WARNING, NULL, &errload[0], 5, 1);
	Info.RestoreScreen(scr);
	res = false;
  };

  delete regionMapperLocal;
  delete parserFactoryLocal;

  return res;
}

void FarEditorSet::ReloadBase()
{
  if (!rEnabled){
    return;
  }

  const wchar_t *marr[2] = { GetMsg(mName), GetMsg(mReloading) };
  HANDLE scr = Info.SaveScreen(0, 0, -1, -1);
  Info.Message(&MainGuid, 0, NULL, &marr[0], 2, 0);

  dropAllEditors(true);
  delete regionMapper;
  delete parserFactory;
  parserFactory = NULL;
  regionMapper = NULL;

  ReadSettings();
  consoleAnnotationAvailable=checkConsoleAnnotationAvailable() && TrueModOn;
  if (consoleAnnotationAvailable){
    hrdClass = DRgb;
    hrdName = sHrdNameTm;
  }
  else{
    hrdClass = DConsole;
    hrdName = sHrdName;
  }

  SString *tpath;
  if (!sCatalogPathExp || !sCatalogPathExp->length()){
    StringBuffer *path=new StringBuffer(PluginPath);
    path->append(DString(FarCatalogXml));
    tpath = path;
  }
  else{
    tpath=sCatalogPathExp;
  }

  try{
    parserFactory = new ParserFactory(tpath);
    delete tpath;
    tpath = NULL;
    hrcParser = parserFactory->getHRCParser();
    LoadUserHrd(sUserHrdPathExp, parserFactory);
    LoadUserHrc(sUserHrcPathExp, parserFactory);
    FarHrcSettings p(parserFactory);
    //p.readProfile();
   // p.readUserProfile();
    defaultType= (FileTypeImpl*)hrcParser->getFileType(&DString("default"));

    try{
      regionMapper = parserFactory->createStyledMapper(&hrdClass, &hrdName);
    }
    catch (ParserFactoryException &e){
      if (getErrorHandler() != NULL){
        getErrorHandler()->error(*e.getMessage());
      }
      regionMapper = parserFactory->createStyledMapper(&hrdClass, NULL);
    };
    //������������� ��� ��������� ��� ������ ������������ ����.
    SetBgEditor();
  }
  catch (Exception &e){
    const wchar_t *errload[5] = { GetMsg(mName), GetMsg(mCantLoad), 0, GetMsg(mFatal), GetMsg(mDie) };

    errload[2] = e.getMessage()->getWChars();

    if (getErrorHandler() != NULL){
      getErrorHandler()->error(*e.getMessage());
    }

    Info.Message(&MainGuid, FMSG_WARNING, NULL, &errload[0], 5, 1);

    delete tpath;
    disableColorer();
  };

  Info.RestoreScreen(scr);
}

ErrorHandler *FarEditorSet::getErrorHandler()
{
  if (parserFactory == NULL){
    return NULL;
  }

  return parserFactory->getErrorHandler();
}

FarEditor *FarEditorSet::addCurrentEditor()
{
  if (viewFirst==1){
    viewFirst=2;
    ReloadBase();
  }

  EditorInfo ei;
  Info.EditorControl(-1, ECTL_GETINFO, NULL, (INT_PTR)&ei);

  FarEditor *editor = new FarEditor(&Info, parserFactory);
  farEditorInstances.put(&SString(ei.EditorID), editor);
  LPWSTR FileName=NULL;
  size_t FileNameSize=Info.EditorControl(-1, ECTL_GETFILENAME, NULL, NULL);

  if (FileNameSize){
    FileName=new wchar_t[FileNameSize];

    if (FileName){
      Info.EditorControl(-1, ECTL_GETFILENAME, NULL, (INT_PTR)FileName);
    }
  }

  DString fnpath(FileName);
  int slash_idx = fnpath.lastIndexOf('\\');

  if (slash_idx == -1){
    slash_idx = fnpath.lastIndexOf('/');
  }

  DString fn = DString(fnpath, slash_idx+1);
  editor->chooseFileType(&fn);
  delete[] FileName;
  editor->setTrueMod(consoleAnnotationAvailable);
  editor->setRegionMapper(regionMapper);
  editor->setDrawCross(drawCross);
  editor->setDrawPairs(drawPairs);
  editor->setDrawSyntax(drawSyntax);
  editor->setOutlineStyle(oldOutline);

  return editor;
}

FarEditor *FarEditorSet::getCurrentEditor()
{
  EditorInfo ei;
  Info.EditorControl(-1, ECTL_GETINFO, NULL, (INT_PTR)&ei);
  FarEditor *editor = farEditorInstances.get(&SString(ei.EditorID));

  return editor;
}

const wchar_t *FarEditorSet::GetMsg(int msg)
{
	return(Info.GetMsg(&MainGuid, msg));
}

void FarEditorSet::enableColorer(bool fromEditor)
{
  rEnabled = true;
  rSetValueDw(0, cRegEnabled, rEnabled);
  ReloadBase();
}

void FarEditorSet::disableColorer()
{
  rEnabled = false;
  rSetValueDw(0, cRegEnabled, rEnabled);

  dropCurrentEditor(true);

  delete regionMapper;
  delete parserFactory;
  parserFactory = NULL;
  regionMapper = NULL;
}

void FarEditorSet::ApplySettingsToEditors()
{
  for (FarEditor *fe = farEditorInstances.enumerate(); fe != NULL; fe = farEditorInstances.next()){
    fe->setTrueMod(consoleAnnotationAvailable);
    fe->setDrawCross(drawCross);
    fe->setDrawPairs(drawPairs);
    fe->setDrawSyntax(drawSyntax);
    fe->setOutlineStyle(oldOutline);
  }
}

void FarEditorSet::dropCurrentEditor(bool clean)
{
  EditorInfo ei;
  Info.EditorControl(-1, ECTL_GETINFO, NULL, (INT_PTR)&ei);
  FarEditor *editor = farEditorInstances.get(&SString(ei.EditorID));
  if (editor){
    if (clean){
      editor->cleanEditor();
    }
    farEditorInstances.remove(&SString(ei.EditorID));
    delete editor;
  }
  Info.EditorControl(-1, ECTL_REDRAW, NULL, NULL);
}

void FarEditorSet::dropAllEditors(bool clean)
{
  if (clean){
    //�� �� ����� ������� � ������ ����������, ����� ��������
    dropCurrentEditor(clean);
  }
  for (FarEditor *fe = farEditorInstances.enumerate(); fe != NULL; fe = farEditorInstances.next()){
    delete fe;
  };

  farEditorInstances.clear();
}

void FarEditorSet::ReadSettings()
{
  const wchar_t *hrdName = rGetValueSz(0, cRegHrdName, cHrdNameDefault);
  const wchar_t *hrdNameTm = rGetValueSz(0, cRegHrdNameTm, cHrdNameTmDefault);
  const wchar_t *catalogPath = rGetValueSz(0, cRegCatalog, cCatalogDefault);
  const wchar_t *userHrdPath = rGetValueSz(0, cRegUserHrdPath, cUserHrdPathDefault);
  const wchar_t *userHrcPath = rGetValueSz(0, cRegUserHrcPath, cUserHrcPathDefault);

  delete sHrdName;
  delete sHrdNameTm;
  delete sCatalogPath;
  delete sCatalogPathExp;
  delete sUserHrdPath;
  delete sUserHrdPathExp;
  delete sUserHrcPath;
  delete sUserHrcPathExp;
  sHrdName = NULL;
  sCatalogPath = NULL;
  sCatalogPathExp = NULL;
  sUserHrdPath = NULL;
  sUserHrdPathExp = NULL;
  sUserHrcPath = NULL;
  sUserHrcPathExp = NULL;

  sHrdName = new SString(DString(hrdName));
  sHrdNameTm = new SString(DString(hrdNameTm));
  sCatalogPath = new SString(DString(catalogPath));
  sCatalogPathExp = PathToFullS(catalogPath,false);
  sUserHrdPath = new SString(DString(userHrdPath));
  sUserHrdPathExp = PathToFullS(userHrdPath,false);
  sUserHrcPath = new SString(DString(userHrcPath));
  sUserHrcPathExp = PathToFullS(userHrcPath,false);

  // two '!' disable "Compiler Warning (level 3) C4800" and slightly faster code
  rEnabled = !!rGetValueDw(0, cRegEnabled, cEnabledDefault);
  drawCross = rGetValueDw(0, cRegCrossDraw, cCrossDrawDefault);
  drawPairs = !!rGetValueDw(0, cRegPairsDraw, cPairsDrawDefault);
  drawSyntax = !!rGetValueDw(0, cRegSyntaxDraw, cSyntaxDrawDefault);
  oldOutline = !!rGetValueDw(0, cRegOldOutLine, cOldOutLineDefault);
  TrueModOn = !!rGetValueDw(0, cRegTrueMod, cTrueMod);
  ChangeBgEditor = !!rGetValueDw(0, cRegChangeBgEditor, cChangeBgEditor);
}


void FarEditorSet::SaveSettings()
{
  rSetValueDw(0, cRegEnabled, rEnabled); 
  rSetValueSz(0, cRegHrdName, sHrdName->getWChars());
  rSetValueSz(0, cRegHrdNameTm, sHrdNameTm->getWChars());
  rSetValueSz(0, cRegCatalog,  sCatalogPath->getWChars());
  rSetValueDw(0, cRegCrossDraw, drawCross); 
  rSetValueDw(0, cRegPairsDraw, drawPairs); 
  rSetValueDw(0, cRegSyntaxDraw, drawSyntax); 
  rSetValueDw(0, cRegOldOutLine, oldOutline); 
  rSetValueDw(0, cRegTrueMod, TrueModOn); 
  rSetValueDw(0, cRegChangeBgEditor, ChangeBgEditor); 
  rSetValueSz(0, cRegUserHrdPath, sUserHrdPath->getWChars());
  rSetValueSz(0, cRegUserHrcPath, sUserHrcPath->getWChars());
}

bool FarEditorSet::checkConEmu()
{
  bool conemu;
  wchar_t shareName[255];
  wsprintf(shareName, AnnotationShareName, sizeof(AnnotationInfo), GetConsoleWindow());

  HANDLE hSharedMem = OpenFileMapping( FILE_MAP_ALL_ACCESS, FALSE, shareName);
  conemu = (hSharedMem != 0) ? 1 : 0;
  CloseHandle(hSharedMem);
  return conemu;
}

bool FarEditorSet::checkFarTrueMod()
{
  EditorAnnotation ea;
  ea.StringNumber = 1;
  ea.StartPos = 1;
  ea.EndPos = 2;
  return !!Info.EditorControl(-1, ECTL_ADDANNOTATION, NULL, (INT_PTR)&ea);
}

bool FarEditorSet::checkConsoleAnnotationAvailable()
{
  return checkConEmu()&&checkFarTrueMod();
}

bool FarEditorSet::SetBgEditor()
{
  if (rEnabled && ChangeBgEditor && !consoleAnnotationAvailable){
    FarSetColors fsc;
    unsigned char c;

    const StyledRegion* def_text=StyledRegion::cast(regionMapper->getRegionDefine(DString("def:Text")));
    c=(def_text->back<<4) + def_text->fore;

    fsc.Flags=FCLR_REDRAW;
    fsc.ColorCount=1;
    fsc.StartIndex=COL_EDITORTEXT;
    fsc.Colors=&c;
    return !!Info.AdvControl(&MainGuid,ACTL_SETARRAYCOLOR,&fsc);
  }
  return false;
}

void FarEditorSet::LoadUserHrd(const String *filename, ParserFactory *pf)
{
  if (filename && filename->length()){
    DocumentBuilder docbuilder;
    Document *xmlDocument;
    InputSource *dfis = InputSource::newInstance(filename);
    xmlDocument = docbuilder.parse(dfis);
    Node *types = xmlDocument->getDocumentElement();

    if (*types->getNodeName() != "hrd-sets"){
      docbuilder.free(xmlDocument);
      throw Exception(DString("main '<hrd-sets>' block not found"));
    }
    for (Node *elem = types->getFirstChild(); elem; elem = elem->getNextSibling()){
      if (elem->getNodeType() == Node::ELEMENT_NODE && *elem->getNodeName() == "hrd"){
        pf->parseHRDSetsChild(elem);
      }
    };
    docbuilder.free(xmlDocument);
  }

}

void FarEditorSet::LoadUserHrc(const String *filename, ParserFactory *pf)
{
  if (filename && filename->length()){
    HRCParser *hr = pf->getHRCParser();
    InputSource *dfis = InputSource::newInstance(filename, NULL);
    try{
      hr->loadSource(dfis);
      delete dfis;
    }catch(Exception &e){
      delete dfis;
      throw Exception(e);
    }
  }
}

const String *FarEditorSet::getParamDefValue(FileTypeImpl *type, SString param)
{
  const String *value;
  value = type->getParamDefaultValue(param);
  if (value == NULL) value = defaultType->getParamValue(param);
  StringBuffer *p=new StringBuffer("<default-");
  p->append(DString(value));
  p->append(DString(">"));
  return p;
}

FarList *FarEditorSet::buildHrcList()
{
  int num = getCountFileTypeAndGroup();;
  const String *group = NULL;
  FileType *type = NULL;

  FarListItem *hrcList = new FarListItem[num];
  memset(hrcList, 0, sizeof(FarListItem)*(num));
  group = NULL;

  for (int idx = 0, i = 0;; idx++, i++){
    type = hrcParser->enumerateFileTypes(idx);

    if (type == NULL){
      break;
    }

    if (group != NULL && !group->equals(type->getGroup())){
      hrcList[i].Flags= LIF_SEPARATOR;
      i++;
    };

    group = type->getGroup();

    const wchar_t *groupChars = NULL;

    if (group != NULL){
      groupChars = group->getWChars();
    }
    else{
      groupChars = L"<no group>";
    }

    hrcList[i].Text = new wchar_t[255];
    _snwprintf((wchar_t*)hrcList[i].Text, 255, L"%s: %s", groupChars, type->getDescription()->getWChars());
  };

  hrcList[0].Flags=LIF_SELECTED;
  FarList *ListItems = new FarList;
  ListItems->Items=hrcList;
  ListItems->ItemsNumber=num;

  return ListItems;
}

FarList *FarEditorSet::buildParamsList(FileTypeImpl *type)
{
  //max count params
  size_t size = type->getParamCount()+defaultType->getParamCount();
  FarListItem *fparam= new FarListItem[size];
  memset(fparam, 0, sizeof(FarListItem)*(size));

  int count=0;
  const String *paramname;
  for(int idx=0;;idx++){
    paramname=defaultType->enumerateParameters(idx);
    if (paramname==NULL){
      break;
    }
    fparam[count++].Text=paramname->getWChars();
  }
  for(int idx=0;;idx++){
    paramname=type->enumerateParameters(idx);
    if (paramname==NULL){
      break;
    }
    if (defaultType->getParamValue(*paramname)==null){
      fparam[count++].Text=paramname->getWChars();
    }
  }

  fparam[0].Flags=LIF_SELECTED;
  FarList *lparam = new FarList;
  lparam->Items=fparam;
  lparam->ItemsNumber=count;
  return lparam;

}

void FarEditorSet::ChangeParamValueListType(HANDLE hDlg, bool dropdownlist)
{
  struct FarDialogItem *DialogItem = (FarDialogItem *) malloc(Info.SendDlgMessage(hDlg,DM_GETDLGITEM,IDX_CH_PARAM_VALUE_LIST,NULL));

  Info.SendDlgMessage(hDlg,DM_GETDLGITEM,IDX_CH_PARAM_VALUE_LIST,(LONG_PTR)DialogItem);
  DialogItem->Flags=DIF_LISTWRAPMODE;
  if (dropdownlist) {
    DialogItem->Flags|=DIF_DROPDOWNLIST;
  }
  Info.SendDlgMessage(hDlg,DM_SETDLGITEM,IDX_CH_PARAM_VALUE_LIST,(LONG_PTR)DialogItem);

  free(DialogItem); 

}

void FarEditorSet::setCrossValueListToCombobox(FileTypeImpl *type, HANDLE hDlg)
{
  const String *value=((FileTypeImpl*)type)->getParamNotDefaultValue(DShowCross);
  const String *def_value=getParamDefValue(type,DShowCross);

  int count = 5;
  FarListItem *fcross = new FarListItem[count];
  memset(fcross, 0, sizeof(FarListItem)*(count));
  fcross[0].Text = DNone.getWChars();
  fcross[1].Text = DVertical.getWChars();
  fcross[2].Text = DHorizontal.getWChars();
  fcross[3].Text = DBoth.getWChars();
  fcross[4].Text = def_value->getWChars();
  FarList *lcross = new FarList;
  lcross->Items=fcross;
  lcross->ItemsNumber=count;

  int ret=4;
  if (value==NULL || !value->length()){
    ret=4;
  }else{
    if (value->equals(&DNone)){
      ret=0;
    }else 
      if (value->equals(&DVertical)){
        ret=1;
      }else
        if (value->equals(&DHorizontal)){
          ret=2;
        }else
          if (value->equals(&DBoth)){
            ret=3;
          }
  };
  fcross[ret].Flags=LIF_SELECTED;
  ChangeParamValueListType(hDlg,true);
  Info.SendDlgMessage(hDlg,DM_LISTSET,IDX_CH_PARAM_VALUE_LIST,(LONG_PTR)lcross);
  delete def_value;
  delete[] fcross;
  delete lcross;
}

void FarEditorSet::setCrossPosValueListToCombobox(FileTypeImpl *type, HANDLE hDlg)
{
  const String *value=type->getParamNotDefaultValue(DCrossZorder);
  const String *def_value=getParamDefValue(type,DCrossZorder);

  int count = 3;
  FarListItem *fcross = new FarListItem[count];
  memset(fcross, 0, sizeof(FarListItem)*(count));
  fcross[0].Text = DBottom.getWChars();
  fcross[1].Text = DTop.getWChars();
  fcross[2].Text = def_value->getWChars();
  FarList *lcross = new FarList;
  lcross->Items=fcross;
  lcross->ItemsNumber=count;

  int ret=2;
  if (value==NULL || !value->length()){
    ret=2;
  }else{
    if (value->equals(&DBottom)){
      ret=0;
    }else 
      if (value->equals(&DTop)){
        ret=1;
      }
  }
  fcross[ret].Flags=LIF_SELECTED;
  ChangeParamValueListType(hDlg,true);
  Info.SendDlgMessage(hDlg,DM_LISTSET,IDX_CH_PARAM_VALUE_LIST,(LONG_PTR)lcross);
  delete def_value;
  delete[] fcross;
  delete lcross;
}

void FarEditorSet::setYNListValueToCombobox(FileTypeImpl *type, HANDLE hDlg, DString param)
{
  const String *value=type->getParamNotDefaultValue(param);
  const String *def_value=getParamDefValue(type,param);

  int count = 3;
  FarListItem *fcross = new FarListItem[count];
  memset(fcross, 0, sizeof(FarListItem)*(count));
  fcross[0].Text = DNo.getWChars();
  fcross[1].Text = DYes.getWChars();
  fcross[2].Text = def_value->getWChars();
  FarList *lcross = new FarList;
  lcross->Items=fcross;
  lcross->ItemsNumber=count;

  int ret=2;
  if (value==NULL || !value->length()){
    ret=2;
  }else{
    if (value->equals(&DNo)){
      ret=0;
    }else 
      if (value->equals(&DYes)){
        ret=1;
      }
  }
  fcross[ret].Flags=LIF_SELECTED;
  ChangeParamValueListType(hDlg,true);
  Info.SendDlgMessage(hDlg,DM_LISTSET,IDX_CH_PARAM_VALUE_LIST,(LONG_PTR)lcross);
  delete def_value;
  delete[] fcross;
  delete lcross;
}

void FarEditorSet::setCustomListValueToCombobox(FileTypeImpl *type,HANDLE hDlg, DString param)
{
  const String *value=type->getParamNotDefaultValue(param);
  const String *def_value=getParamDefValue(type,param);

  int count = 1;
  FarListItem *fcross = new FarListItem[count];
  memset(fcross, 0, sizeof(FarListItem)*(count));
  fcross[0].Text = def_value->getWChars();
  FarList *lcross = new FarList;
  lcross->Items=fcross;
  lcross->ItemsNumber=count;

  fcross[0].Flags=LIF_SELECTED;
  ChangeParamValueListType(hDlg,false);
  Info.SendDlgMessage(hDlg,DM_LISTSET,IDX_CH_PARAM_VALUE_LIST,(LONG_PTR)lcross);

  if (value!=NULL){
    Info.SendDlgMessage(hDlg,DM_SETTEXTPTR ,IDX_CH_PARAM_VALUE_LIST,(LONG_PTR)value->getWChars());
  }
  delete def_value;
  delete[] fcross;
  delete lcross;
}

FileTypeImpl *FarEditorSet::getCurrentTypeInDialog(HANDLE hDlg)
{
  int k=(int)Info.SendDlgMessage(hDlg,DM_LISTGETCURPOS,IDX_CH_SCHEMAS,0);
  return getFileTypeByIndex(k);
}

void  FarEditorSet::OnChangeHrc(HANDLE hDlg)
{
  if (menuid != -1){
    SaveChangedValueParam(hDlg);
  }
  FileTypeImpl *type = getCurrentTypeInDialog(hDlg);
  FarList *List=buildParamsList(type);

  Info.SendDlgMessage(hDlg,DM_LISTSET,IDX_CH_PARAM_LIST,(LONG_PTR)List);
  delete[] List->Items;
  delete List;
  OnChangeParam(hDlg,0);
}

void FarEditorSet::SaveChangedValueParam(HANDLE hDlg)
{
  FarListGetItem List;
  List.ItemIndex=menuid;
  Info.SendDlgMessage(hDlg,DM_LISTGETITEM,IDX_CH_PARAM_LIST,(LONG_PTR)&List);

  //param name
  DString p=DString(List.Item.Text);
  //param value 
  DString v=DString(trim((wchar_t*)Info.SendDlgMessage(hDlg,DM_GETCONSTTEXTPTR,IDX_CH_PARAM_VALUE_LIST,0)));
  FileTypeImpl *type = getCurrentTypeInDialog(hDlg);
  const String *value=((FileTypeImpl*)type)->getParamNotDefaultValue(p);
  const String *def_value=getParamDefValue(type,p);
  if (value==NULL || !value->length()){//���� default ��������
    //���� ��� ��������  
    if (!v.equals(def_value)){
      if (type->getParamValue(p)==null){
        ((FileTypeImpl*)type)->addParam(&p);
      }
      type->setParamValue(p,&v);
    }
  }else{//���� ���������������� ��������
    if (!v.equals(value)){//changed
      if (v.equals(def_value)){
         //delete value
        ((FileTypeImpl*)type)->removeParamValue(&p);
      }else{
        type->setParamValue(p,&v);
      }
    }

  }
  delete def_value;
}

void  FarEditorSet::OnChangeParam(HANDLE hDlg, int idx)
{
  if (menuid!=idx && menuid!=-1) {
    SaveChangedValueParam(hDlg);
  }
  FileTypeImpl *type = getCurrentTypeInDialog(hDlg);
  FarListGetItem List;
  List.ItemIndex=idx;
  Info.SendDlgMessage(hDlg,DM_LISTGETITEM,IDX_CH_PARAM_LIST,(LONG_PTR)&List);

  menuid=idx;
  DString p=DString(List.Item.Text);

  const String *value;
  value=type->getParameterDescription(p);
  if (value==NULL){
    value=defaultType->getParameterDescription(p);
  }
  if (value!=NULL){
    Info.SendDlgMessage(hDlg,DM_SETTEXTPTR ,IDX_CH_DESCRIPTION,(LONG_PTR)value->getWChars());
  }

  COORD c;
  c.X=0;
  Info.SendDlgMessage(hDlg,DM_SETCURSORPOS ,IDX_CH_DESCRIPTION,(LONG_PTR)&c);
  if (p.equals(&DShowCross)){
    setCrossValueListToCombobox(type, hDlg);
  }else{
    if (p.equals(&DCrossZorder)){
      setCrossPosValueListToCombobox(type, hDlg);
    }else
      if (p.equals(&DMaxLen)||p.equals(&DBackparse)||p.equals(&DDefFore)||p.equals(&DDefBack)
        ||p.equals("firstlines")||p.equals("firstlinebytes")){
          setCustomListValueToCombobox(type,hDlg,DString(List.Item.Text));        
      }else{        
        setYNListValueToCombobox(type, hDlg,DString(List.Item.Text));
      }
  }

}

void FarEditorSet::OnSaveHrcParams(HANDLE hDlg)
{
   SaveChangedValueParam(hDlg);
   FarHrcSettings p(parserFactory);
   p.writeUserProfile();
}

INT_PTR WINAPI SettingHrcDialogProc(HANDLE hDlg, int Msg, int Param1, INT_PTR Param2) 
{
  FarEditorSet *fes = (FarEditorSet *)Info.SendDlgMessage(hDlg,DM_GETDLGDATA,0,0);; 

  switch (Msg){
    case DN_GOTFOCUS:
      {
        if (fes->dialogFirstFocus){
          fes->menuid = -1;
          fes->OnChangeHrc(hDlg);
          fes->dialogFirstFocus = false;
        }
        return false;
      }
    break;
    case DN_BTNCLICK:
    switch (Param1){
      case IDX_CH_OK:
        fes->OnSaveHrcParams(hDlg);
        return false;
        break;
    }
    break;
    case DN_EDITCHANGE:
    switch (Param1){
      case IDX_CH_SCHEMAS:
        fes->menuid = -1;
        fes->OnChangeHrc(hDlg);
        return true;
        break;
    }
    break;
    case DN_LISTCHANGE:
    switch (Param1){
      case IDX_CH_PARAM_LIST:
        fes->OnChangeParam(hDlg,(int)Param2);
        return true;
        break;
    }
    break;
  }

  return Info.DefDlgProc(hDlg, Msg, Param1, Param2);
}

void FarEditorSet::configureHrc()
{ 
  if (!rEnabled) return;

  FarDialogItem fdi[] =
  {// type, x1, y1, x2, y2, param, history, mask, flags, userdata, ptrdata, maxlen 
    { DI_DOUBLEBOX,2,1,56,21,0,0,0,0,0,L"",0},                            //IDX_CH_BOX,
    { DI_TEXT,3,3,0,3,0,0,0,0,0,L"",0},                                   //IDX_CH_CAPTIONLIST,
    { DI_COMBOBOX,10,3,54,2,0,0,0,0,0,L"",0},                             //IDX_CH_SCHEMAS,
    { DI_LISTBOX,3,4,30,17,0,0,0,0,0,L"",0},                              //IDX_CH_PARAM_LIST,
    { DI_TEXT,32,5,0,5,0,0,0,0,0,L"",0},                                  //IDX_CH_PARAM_VALUE_CAPTION
    { DI_COMBOBOX,32,6,54,6,0,0,0,0,0,L"",0},                             //IDX_CH_PARAM_VALUE_LIST
    { DI_EDIT,4,18,54,18,0,0,0,0,0,L"",0},                                //IDX_CH_DESCRIPTION,
    { DI_BUTTON,37,20,0,0,0,0,0,DIF_DEFAULTBUTTON,0,L"",0},               //IDX_OK,
    { DI_BUTTON,45,20,0,0,0,0,0,0,0,L"",0},                               //IDX_CANCEL,
  };

  fdi[IDX_CH_BOX].PtrData = GetMsg(mUserHrcSettingDialog);
  fdi[IDX_CH_CAPTIONLIST].PtrData = GetMsg(mListSyntax); 
  FarList* l=buildHrcList();
  fdi[IDX_CH_SCHEMAS].ListItems = l;
  fdi[IDX_CH_SCHEMAS].Flags= DIF_LISTWRAPMODE | DIF_DROPDOWNLIST;
  fdi[IDX_CH_OK].PtrData = GetMsg(mOk);
  fdi[IDX_CH_CANCEL].PtrData = GetMsg(mCancel);  
  fdi[IDX_CH_PARAM_LIST].PtrData = GetMsg(mParamList);
  fdi[IDX_CH_PARAM_VALUE_CAPTION].PtrData = GetMsg(mParamValue);
  fdi[IDX_CH_DESCRIPTION].Flags= DIF_READONLY;

  fdi[IDX_CH_PARAM_LIST].Flags= DIF_LISTWRAPMODE | DIF_LISTNOCLOSE;
  fdi[IDX_CH_PARAM_VALUE_LIST].Flags= DIF_LISTWRAPMODE ;

  dialogFirstFocus = true;
  HANDLE hDlg = Info.DialogInit(&MainGuid,&HrcPluginConfig, -1, -1, 59, 23, L"confighrc", fdi, ARRAY_SIZE(fdi), 0, 0, SettingHrcDialogProc, (LONG_PTR)this);
  int i = Info.DialogRun(hDlg);
  
  for (int idx = 0; idx < l->ItemsNumber; idx++){
    if (l->Items[idx].Text){
      delete[] l->Items[idx].Text;
    }
  }
  delete[] l->Items;
  delete l;
  
  Info.DialogFree(hDlg);
}

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Colorer Library.
 *
 * The Initial Developer of the Original Code is
 * Cail Lomecb <cail@nm.ru>.
 * Portions created by the Initial Developer are Copyright (C) 1999-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */