#include "loop_rigger/scripting/PythonScriptHost.h"

#include <Python.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace loop_rigger::scripting {

namespace {

class PythonRuntime {
public:
    PythonRuntime()
    {
        std::lock_guard<std::mutex> lock(mutex());
        if (refCount() == 0 && !Py_IsInitialized()) {
            Py_Initialize();
        }
        ++refCount();
    }

    ~PythonRuntime()
    {
        std::lock_guard<std::mutex> lock(mutex());
        --refCount();
        if (refCount() == 0 && Py_IsInitialized()) {
            Py_Finalize();
        }
    }

private:
    static std::mutex& mutex()
    {
        static std::mutex result;
        return result;
    }

    static std::size_t& refCount()
    {
        static std::size_t result = 0;
        return result;
    }
};

class PythonObject {
public:
    explicit PythonObject(PyObject* object = nullptr)
        : object_(object)
    {
    }

    PythonObject(const PythonObject&) = delete;
    PythonObject& operator=(const PythonObject&) = delete;

    PythonObject(PythonObject&& other) noexcept
        : object_(std::exchange(other.object_, nullptr))
    {
    }

    PythonObject& operator=(PythonObject&& other) noexcept
    {
        if (this != &other) {
            Py_XDECREF(object_);
            object_ = std::exchange(other.object_, nullptr);
        }
        return *this;
    }

    ~PythonObject()
    {
        Py_XDECREF(object_);
    }

    PyObject* get() const
    {
        return object_;
    }

    PyObject* release()
    {
        return std::exchange(object_, nullptr);
    }

private:
    PyObject* object_ = nullptr;
};

std::string readTextFile(const std::string& path)
{
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open Python device script: " + path);
    }

    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

std::string takePythonError()
{
    if (!PyErr_Occurred()) {
        return {};
    }

    PyObject* type = nullptr;
    PyObject* value = nullptr;
    PyObject* traceback = nullptr;
    PyErr_Fetch(&type, &value, &traceback);
    PyErr_NormalizeException(&type, &value, &traceback);

    PythonObject typeObject(type);
    PythonObject valueObject(value);
    PythonObject tracebackObject(traceback);

    PythonObject text(PyObject_Str(valueObject.get() != nullptr ? valueObject.get() : typeObject.get()));
    if (text.get() == nullptr) {
        return "unknown Python error";
    }
    const char* utf8 = PyUnicode_AsUTF8(text.get());
    return utf8 != nullptr ? utf8 : "unknown Python error";
}

ScriptDiagnostic diagnostic(ScriptDiagnosticLevel level, std::string message)
{
    ScriptDiagnostic result;
    result.level = level;
    result.message = std::move(message);
    return result;
}

std::string eventTypeName(control::WidgetEventType type)
{
    switch (type) {
    case control::WidgetEventType::Press:
        return "press";
    case control::WidgetEventType::Release:
        return "release";
    case control::WidgetEventType::Change:
        return "change";
    }
    return "unknown";
}

bool parseCommandType(const std::string& value, core::CommandType& result)
{
    struct Entry {
        const char* name;
        core::CommandType type;
    };
    static constexpr Entry entries[] = {
        {"select_input_preset_page", core::CommandType::SelectInputPresetPage},
        {"select_input_preset", core::CommandType::SelectInputPreset},
        {"set_input_volume", core::CommandType::SetInputVolume},
        {"set_input_fx_level", core::CommandType::SetInputFxLevel},
        {"set_input_fx_parameter", core::CommandType::SetInputFxParameter},
        {"select_looper", core::CommandType::SelectLooper},
        {"select_sample_length", core::CommandType::SelectSampleLength},
        {"toggle_track_recording", core::CommandType::ToggleTrackRecording},
        {"clear_track", core::CommandType::ClearTrack},
        {"set_track_volume", core::CommandType::SetTrackVolume},
        {"set_track_pan", core::CommandType::SetTrackPan},
        {"set_looper_volume", core::CommandType::SetLooperVolume},
        {"toggle_track_selection", core::CommandType::ToggleTrackSelection},
        {"start_resample_selected_looper", core::CommandType::StartResampleSelectedLooper},
        {"start_resample_all_loopers", core::CommandType::StartResampleAllLoopers},
        {"stop_resampling", core::CommandType::StopResampling},
        {"reset_looper", core::CommandType::ResetLooper},
        {"reset_all", core::CommandType::ResetAll},
    };

    const auto found = std::find_if(std::begin(entries), std::end(entries), [&value](const Entry& entry) {
        return value == entry.name;
    });
    if (found == std::end(entries)) {
        return false;
    }
    result = found->type;
    return true;
}

std::string stringField(PyObject* dictionary, const char* name, const std::string& fallback = {})
{
    PyObject* value = PyDict_GetItemString(dictionary, name);
    if (value == nullptr || value == Py_None) {
        return fallback;
    }
    if (!PyUnicode_Check(value)) {
        return fallback;
    }
    const char* utf8 = PyUnicode_AsUTF8(value);
    return utf8 != nullptr ? utf8 : fallback;
}

int intField(PyObject* dictionary, const char* name, int fallback = 0)
{
    PyObject* value = PyDict_GetItemString(dictionary, name);
    if (value == nullptr || value == Py_None) {
        return fallback;
    }
    return static_cast<int>(PyLong_AsLong(value));
}

float floatField(PyObject* dictionary, const char* name, float fallback = 0.0F)
{
    PyObject* value = PyDict_GetItemString(dictionary, name);
    if (value == nullptr || value == Py_None) {
        return fallback;
    }
    return static_cast<float>(PyFloat_AsDouble(value));
}

std::vector<ScriptAction> parseActions(PyObject* value)
{
    std::vector<ScriptAction> actions;
    if (value == nullptr || value == Py_None) {
        return actions;
    }
    if (!PyList_Check(value)) {
        return actions;
    }

    const auto size = PyList_Size(value);
    for (Py_ssize_t index = 0; index < size; ++index) {
        PyObject* item = PyList_GetItem(value, index);
        if (item == nullptr || !PyDict_Check(item)) {
            continue;
        }

        core::CommandType commandType = core::CommandType::ResetAll;
        if (!parseCommandType(stringField(item, "type"), commandType)) {
            continue;
        }

        ScriptAction action;
        action.command.type = commandType;
        action.command.index = intField(item, "index");
        action.command.secondaryIndex = intField(item, "secondaryIndex");
        action.command.value = floatField(item, "value");
        actions.push_back(action);
    }

    return actions;
}

class PythonScriptDevice final : public ScriptDevice {
public:
    PythonScriptDevice(std::shared_ptr<PythonRuntime> runtime, PythonObject globals, std::string path)
        : runtime_(std::move(runtime))
        , globals_(std::move(globals))
        , path_(std::move(path))
    {
    }

    std::string id() const override
    {
        return stringGlobal("DEVICE_ID", std::filesystem::path(path_).stem().string());
    }

    std::string displayName() const override
    {
        return stringGlobal("DISPLAY_NAME", id());
    }

    std::vector<ScriptAction> handleEvent(const ScriptEvent& event) override
    {
        PyGILState_STATE gil = PyGILState_Ensure();
        PyObject* callback = PyDict_GetItemString(globals_.get(), "on_event");
        if (callback == nullptr || !PyCallable_Check(callback)) {
            PyGILState_Release(gil);
            return {};
        }

        PythonObject eventObject(PyDict_New());
        PyDict_SetItemString(eventObject.get(), "deviceId", PythonObject(PyUnicode_FromString(event.deviceId.c_str())).get());
        PyDict_SetItemString(eventObject.get(), "widgetId", PythonObject(PyUnicode_FromString(event.widgetId.c_str())).get());
        PyDict_SetItemString(eventObject.get(), "type", PythonObject(PyUnicode_FromString(eventTypeName(event.type).c_str())).get());
        PyDict_SetItemString(eventObject.get(), "value", PythonObject(PyFloat_FromDouble(event.value)).get());

        PythonObject arguments(PyTuple_New(1));
        PyTuple_SetItem(arguments.get(), 0, eventObject.release());

        PythonObject result(PyObject_CallObject(callback, arguments.get()));
        if (result.get() == nullptr) {
            takePythonError();
            PyGILState_Release(gil);
            return {};
        }

        auto actions = parseActions(result.get());
        PyGILState_Release(gil);
        return actions;
    }

private:
    std::string stringGlobal(const char* name, const std::string& fallback) const
    {
        PyGILState_STATE gil = PyGILState_Ensure();
        PyObject* value = PyDict_GetItemString(globals_.get(), name);
        std::string result = fallback;
        if (value != nullptr && PyUnicode_Check(value)) {
            if (const char* utf8 = PyUnicode_AsUTF8(value)) {
                result = utf8;
            }
        }
        PyGILState_Release(gil);
        return result;
    }

    std::shared_ptr<PythonRuntime> runtime_;
    PythonObject globals_;
    std::string path_;
};

class PythonScriptHost final : public ScriptHost {
public:
    ScriptLoadResult loadDeviceScript(const std::string& scriptPath) override
    {
        ScriptLoadResult result;
        auto runtime = std::make_shared<PythonRuntime>();
        PyGILState_STATE gil = PyGILState_Ensure();

        PythonObject globals(PyDict_New());
        PyDict_SetItemString(globals.get(), "__builtins__", PyEval_GetBuiltins());
        PyDict_SetItemString(globals.get(), "__file__", PythonObject(PyUnicode_FromString(scriptPath.c_str())).get());

        try {
            const auto source = readTextFile(scriptPath);
            PythonObject compiled(Py_CompileString(source.c_str(), scriptPath.c_str(), Py_file_input));
            if (compiled.get() == nullptr) {
                result.diagnostics.push_back(diagnostic(ScriptDiagnosticLevel::Error, takePythonError()));
            } else {
                PythonObject evaluated(PyEval_EvalCode(compiled.get(), globals.get(), globals.get()));
                if (evaluated.get() == nullptr) {
                    result.diagnostics.push_back(diagnostic(ScriptDiagnosticLevel::Error, takePythonError()));
                }
            }
        } catch (const std::exception& error) {
            result.diagnostics.push_back(diagnostic(ScriptDiagnosticLevel::Error, error.what()));
        }

        if (result.diagnostics.empty()) {
            result.device = std::make_shared<PythonScriptDevice>(std::move(runtime), std::move(globals), scriptPath);
        }

        PyGILState_Release(gil);
        return result;
    }
};

} // namespace

std::unique_ptr<ScriptHost> makePythonScriptHost()
{
    return std::make_unique<PythonScriptHost>();
}

} // namespace loop_rigger::scripting
