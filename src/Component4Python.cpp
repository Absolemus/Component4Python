/*
 *  Modern Native AddIn
 *  Copyright (C) 2018  Infactum
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <sstream>
#include <stdexcept>
#include <locale>
#include <codecvt>

#include "Component4Python.h"

std::string Component4Python::extensionName() {
    return "Component4Python";
}

Component4Python::Component4Python() {

    initProperty();
    initMethods();

}

void Component4Python::initProperty() {
    AddProperty(L"Version", L"ВерсияКомпоненты", [&]() {
        auto s = std::string(Version);
        return std::make_shared<variant_t>(std::move(s));
    });
    AddProperty(L"isInitialized", L"ПитонВключен", [&]() {
        return std::make_shared<variant_t>(isInitialized);
    });
    AddProperty(L"pathToVenv", L"ПутьКВиртуальномуОкружению", [&]() {
        return std::make_shared<variant_t>(pathToVenv);
    }, [&](const variant_t &value) {
        pathToVenv = value;
    });
}

void Component4Python::initMethods() {
    AddMethod(L"initializePython", L"ВключитьПитон", this, &Component4Python::initializePython);
    AddMethod(L"uninitializePython", L"ВыключитьПитон", this, &Component4Python::uninitializePython);
    AddMethod(L"MakeSimpleQR", L"СоздатьПростойQR", this, &Component4Python::makeSimpleQR);
    AddMethod(L"Validator", L"Валидатор", this, &Component4Python::validator);
}

void Component4Python::uninitializePython() {
    auto r = Py_FinalizeEx();
    if (r < 0) {
        throw std::runtime_error(u8"Ошибка выключения Python.");
    }

    if (Py_IsInitialized()) {
        throw std::runtime_error(u8"Python не выключен.");
    }

    isInitialized = false;
}

void Component4Python::initializePython() {
    auto ccPath = std::get<std::string>(pathToVenv).c_str();
    if (!ccPath) {
        Py_Initialize();
    } else {
        PyConfig config;
        PyConfig_InitIsolatedConfig(&config);

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        auto wsPath = converter.from_bytes(ccPath);
        auto venv_path = new wchar_t[wsPath.size() + 1];
        std::copy(wsPath.begin(), wsPath.end(), venv_path);
        venv_path[wsPath.size()] = L'\0';
        PyConfig_SetString(&config, &config.executable, venv_path);
        delete[] venv_path;
        auto status = Py_InitializeFromConfig(&config);
        PyConfig_Clear(&config);

        if (PyStatus_Exception(status)) {
            auto ss = std::stringstream();
            auto msg = u8"Ошибка инициализации Python.";
            ss << msg << u8" status.func: " << (status.func ? status.func : "N/A") << u8" status.err_msg: "
               << (status.err_msg ? status.err_msg : "N/A");
            throw std::runtime_error(ss.str());
        }
    }

    if (!Py_IsInitialized()) {
        throw std::runtime_error(u8"Ошибка инициализации Python.");
    }

    isInitialized = true;
}

void Component4Python::exceptionHandler(std::shared_ptr<std::string> msg) {
    auto error = PyErr_GetRaisedException();
    if (!error) {
        throw std::runtime_error(msg->c_str());
    } else {
        auto error_str = PyObject_Str(error);
        auto error_cstr = PyUnicode_AsUTF8(error_str);
        auto ss = std::stringstream();
        ss << msg->c_str() << u8" по причине: " << error_cstr;

        Py_DECREF(error_str);
        Py_DECREF(error);
        throw std::runtime_error(ss.str());
    }
}

std::shared_ptr<std::vector<char>> Component4Python::imgToBytes(PyObject *img) {
    // импортируем модуль io
    auto io = PyImport_ImportModule("io");
    // если модуль не импортировался, то выводим сообщение об ошибке
    if (!io) {
        auto msg = std::make_shared<std::string>(u8"Ошибка при импорте модуля io");
        exceptionHandler(msg);
    }
    // создаем объект BytesIO
    auto bytesio_name = PyUnicode_FromString("BytesIO");
    auto bytesio = PyObject_CallMethodNoArgs(io, bytesio_name);
    // если объект не создался, то выводим сообщение об ошибке
    if (!bytesio) {
        auto msg = std::make_shared<std::string>(u8"Ошибка при создании объекта BytesIO");
        exceptionHandler(msg);
    }
    // преобразуем имя метода save из строки в объект Python
    auto save_method = PyUnicode_FromString("save");
    // вызываем метод save объекта img передавая объект bytesio
    PyObject_CallMethodOneArg(img, save_method, bytesio);
    // преобразуем имя метода getvalue из строки в объект Python
    auto getvalue_method = PyUnicode_FromString("getvalue");
    // вызываем метод getvalue объекта bytesio
    auto image_bytes = PyObject_CallMethodNoArgs(bytesio, getvalue_method);
    // если метод не вернул объект, то выводим сообщение об ошибке
    if (!image_bytes) {
        auto msg = std::make_shared<std::string>(u8"Ошибка при получении байтового представления изображения");
        exceptionHandler(msg);
    }

    // конвертируем объект bytes в std::vector<unsigned char>
    char* buffer;
    Py_ssize_t length;
    PyBytes_AsStringAndSize(image_bytes, &buffer, &length);
    auto result = std::make_shared<std::vector<char>>(buffer, buffer + length);

    // удаляем объекты Python
    Py_DECREF(image_bytes);
    Py_DECREF(bytesio);
    Py_DECREF(io);

    return result;
}

variant_t Component4Python::makeSimpleQR(const variant_t &data) {
    // импортируем модуль qrcode
    auto qrcode = PyImport_ImportModule("qrcode");
    if (!qrcode) {
        auto msg = std::make_shared<std::string>(u8"Ошибка при импорте модуля qrcode");
        exceptionHandler(msg);
    }
    // преобразуем имя метода make из строки в объект Python
    auto make_method = PyUnicode_FromString("make");
    // вызываем метод make передавая строку с данными
    auto img = PyObject_CallMethodOneArg(qrcode, make_method, PyUnicode_FromString(std::get<std::string>(data).c_str()));
    if (!img) {
        auto msg = std::make_shared<std::string>(u8"Ошибка при создании изображения QR кода");
        exceptionHandler(msg);
    }

    // конвертируем объект img в std::vector<unsigned char>
    auto result =  imgToBytes(img);

    // удаляем объекты Python
    Py_DECREF(img);
    Py_DECREF(qrcode);

    return *result;
}

variant_t Component4Python::validator(const variant_t &data, const variant_t &schema) {
    // проверяем параметры на заполненность
    if (std::get<std::string>(data).empty() || std::get<std::string>(schema).empty()) {
        auto msg = std::make_shared<std::string>(u8"Параметры data и schema должны быть заполнены");
        exceptionHandler(msg);
    }

    // импортируем модуль validator
    auto validator = PyImport_ImportModule("schema.validator");
    if (!validator) {
        auto msg = std::make_shared<std::string>(u8"Ошибка при импорте модуля validator");
        exceptionHandler(msg);
    }

    // создаем класс Validator
    auto validator_attr = PyObject_GetAttrString(validator, "Validator");
    auto py_data = PyUnicode_FromString(std::get<std::string>(data).c_str());
    auto py_schema = PyUnicode_FromString(std::get<std::string>(schema).c_str());
    auto args = PyTuple_Pack(2, py_data, py_schema);
    auto validator_class = PyObject_CallObject(validator_attr, args);
    if (!validator_class) {
        auto msg = std::make_shared<std::string>(u8"Ошибка при создании объекта класса Validator");
        exceptionHandler(msg);
    }

    // вызываем метод validate класса Validator
    auto validate_method = PyUnicode_FromString("validate");
    PyObject_CallMethodNoArgs(validator_class, validate_method);

    auto error = PyErr_GetRaisedException();

    if (!error) {
        // удаляем объекты Python
        Py_DECREF(validator_class);
        Py_DECREF(validator_attr);
        Py_DECREF(validator);
        Py_DECREF(args);

        auto res = std::make_shared<std::string>("OK");
        return *res;
    } else {
        auto error_str = PyObject_Str(error);
        auto error_cstr = PyUnicode_AsUTF8(error_str);

        Py_DECREF(error_str);
        Py_DECREF(error);
        Py_DECREF(validator_class);
        Py_DECREF(validator_attr);
        Py_DECREF(validator);
        Py_DECREF(args);
        auto res = std::make_shared<std::string>(error_cstr);
        return *res;
    }
}


