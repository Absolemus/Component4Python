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

#ifndef QRCode41ADDIN_H
#define QRCode41ADDIN_H

#include "Component.h"
#include "Python.h"

class Component4Python final : public Component {
public:
    const char *Version = u8"1.0.0";
    Component4Python();
private:
    std::string extensionName() override;
    variant_t isInitialized;
    variant_t pathToVenv;
    void initializePython();
    void uninitializePython();
    void initMethods();
    void initProperty();

    variant_t makeSimpleQR(const variant_t &data);
    static void exceptionHandler(std::shared_ptr<std::string> msg);
    static std::shared_ptr<std::vector<char>> imgToBytes(PyObject *img);

    variant_t validator(const variant_t &data, const variant_t &schema);
};

#endif //QRCode41ADDIN_H
