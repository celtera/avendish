#ifndef VINTAGE_HPP_55421FF5_8A01_42C3_8DA5_4FA2802ADF9A
#define VINTAGE_HPP_55421FF5_8A01_42C3_8DA5_4FA2802ADF9A

/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <cinttypes>

namespace vintage
{
struct Constants
{
  static constexpr int32_t ApiVersion = 2400;
  static constexpr int32_t ApiMagic = 1450406992;
  static constexpr int32_t ChunkMagic = 1130589771;
  static constexpr int32_t EffectMagic = 1182286699;
  static constexpr int32_t BankMagic = 1182286443;
  static constexpr int32_t ChunkPresetMagic = 1179665256;
  static constexpr int32_t ChunkBankMagic = 1178747752;

  static constexpr int32_t ProgNameLen = 24;
  static constexpr int32_t ParamStrLen = 8;
  static constexpr int32_t VendorStrLen = 64;
  static constexpr int32_t ProductStrLen = 64;
  static constexpr int32_t EffectNameLen = 32;
  static constexpr int32_t NameLen = 64;
  static constexpr int32_t LabelLen = 64;
  static constexpr int32_t ShortLabelLen = 8;
  static constexpr int32_t CategLabelLen = 24;
  static constexpr int32_t FileNameLen = 100;
};

struct HostCanDos
{
  static constexpr const char* SendEvents = "sendVstEvents";
  static constexpr const char* SendMidiEvent = "sendVstMidiEvent";
  static constexpr const char* SendTimeInfo = "sendVstTimeInfo";
  static constexpr const char* ReceiveEvents = "receiveVstEvents";
  static constexpr const char* ReceiveMidiEvent = "receiveVstMidiEvent";
  static constexpr const char* ReportConnectionChanges = "reportConnectionChanges";
  static constexpr const char* AcceptIOChanges = "acceptIOChanges";
  static constexpr const char* SizeWindow = "sizeWindow";
  static constexpr const char* Offline = "offline";
  static constexpr const char* OpenFileSelector = "openFileSelector";
  static constexpr const char* CloseFileSelector = "closeFileSelector";
  static constexpr const char* StartStopProcess = "startStopProcess";
  static constexpr const char* ShellCategory = "shellCategory";
  static constexpr const char* SendMidiEventFlagIsRealtime
      = "sendVstMidiEventFlagIsRealtime";
  static constexpr const char* HasCockosViewAsConfig = "hasCockosViewAsConfig";
};

enum class EffectFlags : int32_t
{
  HasEditor = 1 << 0,
  HasClip = 1 << 1,
  HasVu = 1 << 2,
  CanMono = 1 << 3,
  CanReplacing = 1 << 4,
  ProgramChunks = 1 << 5,
  IsSynth = 1 << 8,
  NoSoundInStop = 1 << 9,
  ExtIsAsync = 1 << 10,
  ExtHasBuffer = 1 << 11,
  CanDoubleReplacing = 1 << 12
};

constexpr EffectFlags operator|(EffectFlags lhs, EffectFlags rhs) noexcept
{
  return EffectFlags(static_cast<int32_t>(lhs) | static_cast<int32_t>(rhs));
}
constexpr EffectFlags& operator|=(EffectFlags& lhs, EffectFlags rhs) noexcept
{
  return lhs = (lhs | rhs);
}

enum class EffectOpcodes : int32_t
{
  Open,
  Close,
  SetProgram,
  GetProgram,
  SetProgramName,
  GetProgramName,
  GetParamLabel,
  GetParamDisplay,
  GetParamName,
  GetVu,
  SetSampleRate,
  SetBlockSize,
  MainsChanged,
  EditGetRect,
  EditOpen,
  EditClose,
  EditDraw,
  EditMouse,
  EditKey,
  EditIdle,
  EditTop,
  EditSleep,
  Identify,
  GetChunk,
  SetChunk,
  ProcessEvents,
  CanBeAutomated,
  String2Parameter,
  GetNumProgramCategories,
  GetProgramNameIndexed,
  CopyProgram,
  ConnectInput,
  ConnectOutput,
  GetInputProperties,
  GetOutputProperties,
  GetPlugCategory,
  GetCurrentPosition,
  GetDestinationBuffer,
  OfflineNotify,
  OfflinePrepare,
  OfflineRun,
  ProcessVarIo,
  SetSpeakerArrangement,
  SetBlockSizeAndSampleRate,
  SetBypass,
  GetEffectName,
  GetErrorText,
  GetVendorString,
  GetProductString,
  GetVendorVersion,
  VendorSpecific,
  CanDo,
  GetTailSize,
  Idle,
  GetIcon,
  SetViewPosition,
  GetParameterProperties,
  KeysRequired,
  GetApiVersion,
  EditKeyDown,
  EditKeyUp,
  SetEditKnobMode,
  GetMidiProgramName,
  GetCurrentMidiProgram,
  GetMidiProgramCategory,
  HasMidiProgramsChanged,
  GetMidiKeyName,
  BeginSetProgram,
  EndSetProgram,
  GetSpeakerArrangement,
  ShellGetNextPlugin,
  StartProcess,
  StopProcess,
  SetTotalSampleToProcess,
  SetPanLaw,
  BeginLoadBank,
  BeginLoadProgram,
  SetProcessPrecision,
  GetNumMidiInputChannels,
  GetNumMidiOutputChannels
};

enum class HostOpcodes : int32_t
{
  Automate,
  Version,
  CurrentId,
  Idle,
  PinConnected,
  WantMidi = PinConnected + 2,
  GetTime,
  ProcessEvents,
  SetTime,
  TempoAt,
  GetNumAutomatableParameters,
  GetParameterQuantization,
  IOChanged,
  NeedIdle,
  SizeWindow,
  GetSampleRate,
  GetBlockSize,
  GetInputLatency,
  GetOutputLatency,
  GetPreviousPlug,
  GetNextPlug,
  WillReplaceOrAccumulate,
  GetCurrentProcessLevel,
  GetAutomationState,
  OfflineStart,
  OfflineRead,
  OfflineWrite,
  OfflineGetCurrentPass,
  OfflineGetCurrentMetaPass,
  SetOutputSampleRate,
  GetOutputSpeakerArrangement,
  GetVendorString,
  GetProductString,
  GetVendorVersion,
  VendorSpecific,
  SetIcon,
  CanDo,
  GetLanguage,
  OpenWindow,
  CloseWindow,
  GetDirectory,
  UpdateDisplay,
  BeginEdit,
  EndEdit,
  OpenFileSelector,
  CloseFileSelector,
  EditFile,
  GetChunkFile,
  GetInputSpeakerArrangement
};

enum class EventTypes : int32_t
{
  Midi = 1,
  Audio,
  Video,
  Parameter,
  Trigger,
  SysEx
};

enum class MidiEventFlags : int32_t
{
  Realtime = 1 << 0
};

enum class TimeInfoFlags : int32_t
{
  TransportChanged = 1 << 0,
  TransportPlaying = 1 << 1,
  TransportCycleActive = 1 << 2,
  TransportRecording = 1 << 3,
  AutomationWriting = 1 << 6,
  AutomationReading = 1 << 7,
  NanosValid = 1 << 8,
  PpqPosValid = 1 << 9,
  TempoValid = 1 << 10,
  BarsValid = 1 << 11,
  CyclePosValid = 1 << 12,
  TimeSigValid = 1 << 13,
  SmpteValid = 1 << 14,
  ClockValid = 1 << 15
};

constexpr TimeInfoFlags operator|(TimeInfoFlags lhs, TimeInfoFlags rhs) noexcept
{
  return TimeInfoFlags(static_cast<int32_t>(lhs) | static_cast<int32_t>(rhs));
}

enum class SmpteFrameRate : int32_t
{
  Smpte24fps = 0,
  Smpte25fps = 1,
  Smpte2997fps = 2,
  Smpte30fps = 3,
  Smpte2997dfps = 4,
  Smpte30dfps = 5,
  SmpteFilm16mm = 6,
  SmpteFilm35mm = 7,
  Smpte239fps = 10,
  Smpte249fps = 11,
  Smpte599fps = 12,
  Smpte60fps = 13
};

enum class HostLanguage : int32_t
{
  English = 1,
  German,
  French,
  Italian,
  Spanish,
  Japanese
};

enum class ProcessPrecision : int32_t
{
  Single = 0,
  Double
};

enum class ParameterFlags : int32_t
{
  IsSwitch = 1 << 0,
  UsesIntegerMinMax = 1 << 1,
  UsesFloatStep = 1 << 2,
  UsesIntStep = 1 << 3,
  SupportsDisplayIndex = 1 << 4,
  SupportsDisplayCategory = 1 << 5,
  CanRamp = 1 << 6
};

constexpr ParameterFlags operator|(ParameterFlags lhs, ParameterFlags rhs) noexcept
{
  return ParameterFlags(static_cast<int32_t>(lhs) | static_cast<int32_t>(rhs));
}

enum class PinPropertiesFlags : int32_t
{
  Active = 1 << 0,
  Stereo = 1 << 1,
  UseSpeaker = 1 << 2
};

constexpr PinPropertiesFlags
operator|(PinPropertiesFlags lhs, PinPropertiesFlags rhs) noexcept
{
  return PinPropertiesFlags(static_cast<int32_t>(lhs) | static_cast<int32_t>(rhs));
}

enum class PlugCategory : int32_t
{
  Unknown,
  Effect,
  Synth,
  Analysis,
  Mastering,
  Spatializer,
  RoomFx,
  SurroundFx,
  Restoration,
  OfflineProcess,
  Shell,
  Generator,
  MaxCount
};

enum class MidiProgramNameFlags : int32_t
{
  Omni = 1
};

constexpr MidiProgramNameFlags
operator|(MidiProgramNameFlags lhs, MidiProgramNameFlags rhs) noexcept
{
  return MidiProgramNameFlags(static_cast<int32_t>(lhs) | static_cast<int32_t>(rhs));
}

enum class SpeakerType : int32_t
{
  Undefined = 0x7fffffff,

  M = 0,
  L,
  R,
  C,
  Lfe,
  Ls,
  Rs,
  Lc,
  Rc,
  S,
  Cs = S,
  Sl,
  Sr,
  Tm,
  Tfl,
  Tfc,
  Tfr,
  Trl,
  Trc,
  Trr,
  Lfe2,

  U32 = -32,
  U31,
  U30,
  U29,
  U28,
  U27,
  U26,
  U25,
  U24,
  U23,
  U22,
  U21,
  U20,
  U19,
  U18,
  U17,
  U16,
  U15,
  U14,
  U13,
  U12,
  U11,
  U10,
  U9,
  U8,
  U7,
  U6,
  U5,
  U4,
  U3,
  U2,
  U1
};

enum class SpeakerArrangementType : int32_t
{
  UserDefined = -2,
  Empty = -1,
  Mono = 0,
  Stereo,
  StereoSurround,
  StereoCenter,
  StereoSide,
  StereoCLfe,
  Cine30,
  Music30,
  Cine31,
  Music31,
  Cine40,
  Music40,
  Cine41,
  Music41,
  Cine50,
  Music50 = Cine50,
  Cine51,
  Music51 = Cine51,
  Cine60,
  Music60,
  Cine61,
  Music61,
  Cine70,
  Music70,
  Cine71,
  Music71,
  Cine80,
  Music80,
  Cine81,
  Music81,
  Cine102,
  Music102 = Cine102,
  Count
};

enum class OfflineTaskFlags : int32_t
{
  InvalidParameter = 1 << 0,
  NewFile = 1 << 1,
  PlugError = 1 << 10,
  InterleavedAudio = 1 << 11,
  TempOutputFile = 1 << 12,
  FloatOutputFile = 1 << 13,
  RandomWrite = 1 << 14,
  Stretch = 1 << 15,
  NoThread = 1 << 16
};

constexpr OfflineTaskFlags operator|(OfflineTaskFlags lhs, OfflineTaskFlags rhs) noexcept
{
  return OfflineTaskFlags(static_cast<int32_t>(lhs) | static_cast<int32_t>(rhs));
}

enum class OfflineOption : int32_t
{
  Audio,
  Peaks,
  Parameter,
  Marker,
  Cursor,
  Selection,
  QueryFiles
};

enum class AudioFileFlags : int32_t
{
  ReadOnly = 1 << 0,
  NoRateConversion = 1 << 1,
  NoChannelChange = 1 << 2,
  CanProcessSelection = 1 << 10,
  NoCrossfade = 1 << 11,
  WantRead = 1 << 12,
  WantWrite = 1 << 13,
  WantWriteMarker = 1 << 14,
  WantMoveCursor = 1 << 15,
  WantSelect = 1 << 16
};

constexpr AudioFileFlags operator|(AudioFileFlags lhs, AudioFileFlags rhs) noexcept
{
  return AudioFileFlags(static_cast<int32_t>(lhs) | static_cast<int32_t>(rhs));
}

enum class VirtualKey : unsigned char
{
  BACK = 1,
  TAB,
  CLEAR,
  RETURN,
  PAUSE,
  ESCAPE,
  SPACE,
  NEXT,
  END,
  HOME,
  LEFT,
  UP,
  RIGHT,
  DOWN,
  PAGEUP,
  PAGEDOWN,
  SELECT,
  PRINT,
  ENTER,
  SNAPSHOT,
  INSERT,
  DELETE,
  HELP,
  NUMPAD0,
  NUMPAD1,
  NUMPAD2,
  NUMPAD3,
  NUMPAD4,
  NUMPAD5,
  NUMPAD6,
  NUMPAD7,
  NUMPAD8,
  NUMPAD9,
  MULTIPLY,
  ADD,
  SEPARATOR,
  SUBTRACT,
  DECIMAL,
  DIVIDE,
  F1,
  F2,
  F3,
  F4,
  F5,
  F6,
  F7,
  F8,
  F9,
  F10,
  F11,
  F12,
  NUMLOCK,
  SCROLL,
  SHIFT,
  CONTROL,
  ALT,
  EQUALS
};

enum class ModifierKey : unsigned char
{
  SHIFT = 1 << 0,
  ALTERNATE = 1 << 1,
  COMMAND = 1 << 2,
  CONTROL = 1 << 3
};

constexpr ModifierKey operator|(ModifierKey lhs, ModifierKey rhs) noexcept
{
  return ModifierKey(static_cast<unsigned char>(lhs) | static_cast<unsigned char>(rhs));
}

enum class FileSelectCommand : int32_t
{
  Load,
  Save,
  MultipleFilesLoad,
  DirectorySelect
};

enum class FileSelectType : int32_t
{
  File
};

enum class PanLaw : int32_t
{
  Linear,
  EqualPower
};

enum class ProcessLevels : int32_t
{
  Unknown,
  User,
  Realtime,
  Prefetch,
  Offline
};

enum class AutomationStates : int32_t
{
  Unsupported,
  Off,
  Read,
  Write,
  ReadWrite
};

enum class EventFlags : int32_t
{
};

constexpr EventFlags operator|(EventFlags lhs, EventFlags rhs) noexcept
{
  return EventFlags(static_cast<int32_t>(lhs) | static_cast<int32_t>(rhs));
}

enum class MidiSysexFlags : int32_t
{
};

constexpr MidiSysexFlags operator|(MidiSysexFlags lhs, MidiSysexFlags rhs) noexcept
{
  return MidiSysexFlags(static_cast<int32_t>(lhs) | static_cast<int32_t>(rhs));
}

enum class MidiProgramCategoryFlags : int32_t
{
};

constexpr MidiProgramCategoryFlags
operator|(MidiProgramCategoryFlags lhs, MidiProgramCategoryFlags rhs) noexcept
{
  return MidiProgramCategoryFlags(static_cast<int32_t>(lhs) | static_cast<int32_t>(rhs));
}

enum class MidiKeyNameFlags : int32_t
{
};

constexpr MidiKeyNameFlags operator|(MidiKeyNameFlags lhs, MidiKeyNameFlags rhs) noexcept
{
  return MidiKeyNameFlags(static_cast<int32_t>(lhs) | static_cast<int32_t>(rhs));
}

struct Rect
{
  int16_t top{};
  int16_t left{};
  int16_t bottom{};
  int16_t right{};
};

struct Event
{
  EventTypes type{};
  int32_t byteSize{};
  int32_t deltaFrames{};
  EventFlags flags{};

  char data[16]{};
};

struct Events
{
  int32_t numEvents{};
  intptr_t reserved{};
  Event* events[2]{};
};

struct MidiEvent
{
  EventTypes type = EventTypes::Midi;
  int32_t byteSize = sizeof(MidiEvent);
  int32_t deltaFrames{};
  MidiEventFlags flags{};

  int32_t noteLength{};
  int32_t noteOffset{};
  char midiData[4]{};
  char detune{};
  char noteOffVelocity{};
  char reserved1{};
  char reserved2{};
};

struct MidiSysexEvent
{
  EventTypes type = EventTypes::SysEx;
  int32_t byteSize = sizeof(MidiSysexEvent);
  int32_t deltaFrames{};
  MidiSysexFlags flags{};

  int32_t dumpBytes{};
  intptr_t resvd1{};
  char* sysexDump{};
  intptr_t resvd2{};
};

struct TimeInfo
{
  double samplePos{};
  double sampleRate{};
  double nanoSeconds{};
  double ppqPos{};
  double tempo{};
  double barStartPos{};
  double cycleStartPos{};
  double cycleEndPos{};
  int32_t timeSigNumerator{};
  int32_t timeSigDenominator{};
  int32_t smpteOffset{};
  SmpteFrameRate smpteFrameRate{};
  int32_t samplesToNextClock{};
  TimeInfoFlags flags{};
};

struct PatchChunkInfo
{
  int32_t version{1};
  int32_t pluginUniqueID{};
  int32_t pluginVersion{};
  int32_t numElements{};
  char future[48]{};
};

struct FileType
{
  char name[128]{};
  char macType[8]{};
  char dosType[8]{};
  char unixType[8]{};
  char mimeType1[128]{};
  char mimeType2[128]{};
};

struct FileSelect
{
  FileSelectCommand command{};
  FileSelectType type{};
  int32_t macCreator{};
  int32_t nbFileTypes{};
  FileType* fileTypes{};
  char title[1024]{};
  char* initialPath{};
  char* returnPath{};
  int32_t sizeReturnPath{};
  char** returnMultiplePaths{};
  int32_t nbReturnPath{};
  intptr_t reserved{};
  char future[116]{};
};

struct VariableIo
{
  float** inputs{};
  float** outputs{};
  int32_t numSamplesInput{};
  int32_t numSamplesOutput{};
  int32_t* numSamplesInputProcessed{};
  int32_t* numSamplesOutputProcessed{};
};

struct ParameterProperties
{
  float stepFloat{};
  float smallStepFloat{};
  float largeStepFloat{};
  char label[Constants::LabelLen]{};
  ParameterFlags flags{};
  int32_t minInteger{};
  int32_t maxInteger{};
  int32_t stepInteger{};
  int32_t largeStepInteger{};
  char shortLabel[Constants::ShortLabelLen]{};
  int16_t displayIndex{};
  int16_t category{};
  int16_t numParametersInCategory{};
  int16_t reserved{};
  char categoryLabel[Constants::CategLabelLen]{};
  char future[16]{};
};

struct PinProperties
{
  char label[Constants::LabelLen]{};
  PinPropertiesFlags flags{};
  SpeakerArrangementType arrangementType{};
  char shortLabel[Constants::ShortLabelLen]{};
  char future[48]{};
};

struct SpeakerProperties
{
  float azimuth{};
  float elevation{};
  float radius{};
  float reserved{};
  char name[Constants::NameLen]{};
  SpeakerType type{};
  char future[28]{};
};

struct SpeakerArrangement
{
  SpeakerArrangementType type{};
  int32_t numChannels{};
  SpeakerProperties speakers[8]{};
};

struct MidiProgramCategory
{
  int32_t thisCategoryIndex{};
  char name[Constants::NameLen]{};
  int32_t parentCategoryIndex{-1};
  MidiProgramCategoryFlags flags{};
};

struct MidiKeyName
{
  int32_t thisProgramIndex{};
  int32_t thisKeyNumber{};
  char keyName[Constants::NameLen]{};
  int32_t reserved{};
  MidiKeyNameFlags flags{};
};

struct MidiProgramName
{
  int32_t thisProgramIndex{};
  char name[Constants::NameLen]{};
  char midiProgram = -1;
  char midiBankMsb = -1;
  char midiBankLsb = -1;
  char reserved{};
  int32_t parentCategoryIndex{-1};
  MidiProgramNameFlags flags{};
};

struct OfflineTask
{
  char processName[96]{};
  double readPosition{};
  double writePosition{};
  int32_t readCount{};
  int32_t writeCount{};
  int32_t sizeInputBuffer{};
  int32_t sizeOutputBuffer{};
  void* inputBuffer{};
  void* outputBuffer{};
  double positionToProcessFrom{};
  double numFramesToProcess{};
  double maxFramesToWrite{};
  void* extraBuffer{};
  int32_t value{};
  int32_t index{};
  double numFramesInSourceFile{};
  double sourceSampleRate{};
  double destinationSampleRate{};
  int32_t numSourceChannels{};
  int32_t numDestinationChannels{};
  int32_t sourceFormat{};
  int32_t destinationFormat{};
  char outputText[512]{};
  double progress{};
  int32_t progressMode{};
  char progressText[100]{};
  OfflineTaskFlags flags{};
  int32_t returnValue{};
  void* hostOwned{};
  void* plugOwned{};
  char future[1024]{};
};

struct AudioFile
{
  AudioFileFlags flags{};
  void* hostOwned{};
  void* plugOwned{};
  char name[Constants::FileNameLen]{};
  int32_t uniqueId{};
  double sampleRate{};
  int32_t numChannels{};
  double numFrames{};
  int32_t format{};
  double editCursorPosition{-1.0};
  double selectionStart{};
  double selectionSize{};
  int32_t selectedChannelsMask{};
  int32_t numMarkers{};
  int32_t timeRulerUnit{};
  double timeRulerOffset{};
  double tempo{};
  int32_t timeSigNumerator{};
  int32_t timeSigDenominator{};
  int32_t ticksPerBlackNote{};
  SmpteFrameRate smpteFrameRate{};
  char future[64]{};
};

struct AudioFileMarker
{
  double position{};
  char name[32]{};
  int32_t type{};
  int32_t id{};
  int32_t reserved{};
};

struct Window
{
  char title[128]{};
  int16_t xPos{};
  int16_t yPos{};
  int16_t width{};
  int16_t height{};
  int32_t style{};
  void* parent{};
  void* userHandle{};
  void* winHandle{};
  char future[104]{};
};

struct KeyCode
{
  int32_t character{};
  VirtualKey virt{};
  ModifierKey modifier{};
};

struct Program
{
  int32_t chunkMagic = Constants::ChunkMagic;
  int32_t byteSize{};
  int32_t fxMagic{};
  int32_t version = 1;
  int32_t fxID{};
  int32_t fxVersion{};
  int32_t numParams{};
  char prgName[28]{};

  union
  {
    float params[1];
    struct
    {
      int32_t size{};
      char chunk[1]{};
    } data;
  } content{};
};

struct Bank
{
  int32_t chunkMagic = Constants::ChunkMagic;
  int32_t byteSize{};
  int32_t fxMagic{};
  int32_t version{};
  int32_t fxID{};
  int32_t fxVersion{};
  int32_t numPrograms{};
  int32_t currentProgram{};
  char future[124]{};

  union
  {
    Program programs[1];
    struct
    {
      int32_t size{};
      char chunk[1]{};
    } data;
  } content{};
};

struct Effect;

using HostCallback = intptr_t (*)(
    Effect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt);
using EffectDispatcherProc = intptr_t (*)(
    Effect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt);
using EffectProcessProc
    = void (*)(Effect* effect, float** inputs, float** outputs, int32_t sampleFrames);
using EffectProcessDoubleProc
    = void (*)(Effect* effect, double** inputs, double** outputs, int32_t sampleFrames);
using EffectSetParameterProc = void (*)(Effect* effect, int32_t index, float parameter);
using EffectGetParameterProc = float (*)(Effect* effect, int32_t index);

struct Effect
{
  int32_t magic = Constants::ApiMagic;
  EffectDispatcherProc dispatcher{};
  EffectProcessProc process{};
  EffectSetParameterProc setParameter{};
  EffectGetParameterProc getParameter{};
  int32_t numPrograms{};
  int32_t numParams{};
  int32_t numInputs{};
  int32_t numOutputs{};
  EffectFlags flags{};
  intptr_t resvd1{};
  intptr_t resvd2{};
  int32_t initialDelay{};
  int32_t realQualities{};
  int32_t offQualities{};
  float ioRatio{};
  void* object{};
  void* user{};
  int32_t uniqueID{};
  int32_t version{};
  EffectProcessProc processReplacing{};
  EffectProcessDoubleProc processDoubleReplacing{};
  char unused[60]{};
};
}

#endif
