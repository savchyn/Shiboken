/*
 * This file is part of the Shiboken Python Bindings Generator project.
 *
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Contact: PySide team <contact@pyside.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include "shibokengenerator.h"
#include <reporthandler.h>

#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <limits>

#define NULL_VALUE "NULL"
#define PARENT_CTOR_HEURISTIC "enable-parent-ctor-heuristic"

static Indentor INDENT;
//static void dumpFunction(AbstractMetaFunctionList lst);

QHash<QString, QString> ShibokenGenerator::m_pythonPrimitiveTypeName = QHash<QString, QString>();
QHash<QString, QString> ShibokenGenerator::m_pythonOperators = QHash<QString, QString>();
QHash<QString, QString> ShibokenGenerator::m_formatUnits = QHash<QString, QString>();
QHash<QString, QString> ShibokenGenerator::m_tpFuncs = QHash<QString, QString>();

ShibokenGenerator::ShibokenGenerator() : Generator()
{
    if (m_pythonPrimitiveTypeName.isEmpty())
        ShibokenGenerator::initPrimitiveTypesCorrespondences();

    if (m_tpFuncs.isEmpty())
        ShibokenGenerator::clearTpFuncs();
}

void ShibokenGenerator::clearTpFuncs()
{
    m_tpFuncs["__str__"] = QString("0");
    m_tpFuncs["__repr__"] = QString("0");
}

void ShibokenGenerator::initPrimitiveTypesCorrespondences()
{
    // Python primitive types names
    m_pythonPrimitiveTypeName.clear();

    // PyBool
    m_pythonPrimitiveTypeName["bool"] = "PyBool";

    // PyInt
    m_pythonPrimitiveTypeName["char"] = "PyInt";
    m_pythonPrimitiveTypeName["signed char"] = "PyInt";
    m_pythonPrimitiveTypeName["unsigned char"] = "PyInt";
    m_pythonPrimitiveTypeName["int"] = "PyInt";
    m_pythonPrimitiveTypeName["signed int"] = "PyInt";
    m_pythonPrimitiveTypeName["uint"] = "PyInt";
    m_pythonPrimitiveTypeName["unsigned int"] = "PyInt";
    m_pythonPrimitiveTypeName["short"] = "PyInt";
    m_pythonPrimitiveTypeName["ushort"] = "PyInt";
    m_pythonPrimitiveTypeName["unsigned short"] = "PyInt";
    m_pythonPrimitiveTypeName["long"] = "PyInt";

    // PyFloat
    m_pythonPrimitiveTypeName["double"] = "PyFloat";
    m_pythonPrimitiveTypeName["float"] = "PyFloat";

    // PyLong
    m_pythonPrimitiveTypeName["unsigned long"] = "PyLong";
    m_pythonPrimitiveTypeName["signed long"] = "PyLong";
    m_pythonPrimitiveTypeName["ulong"] = "PyLong";
    m_pythonPrimitiveTypeName["long long"] = "PyLong";
    m_pythonPrimitiveTypeName["__int64"] = "PyLong";
    m_pythonPrimitiveTypeName["unsigned long long"] = "PyLong";
    m_pythonPrimitiveTypeName["unsigned __int64"] = "PyLong";

    // Python operators
    m_pythonOperators.clear();

    // Arithmetic operators
    m_pythonOperators["operator+"] = "add";
    m_pythonOperators["operator-"] = "sub";
    m_pythonOperators["operator*"] = "mul";
    m_pythonOperators["operator/"] = "div";
    m_pythonOperators["operator%"] = "mod";

    // Inplace arithmetic operators
    m_pythonOperators["operator+="] = "iadd";
    m_pythonOperators["operator-="] = "isub";
    m_pythonOperators["operator*="] = "imul";
    m_pythonOperators["operator/="] = "idiv";
    m_pythonOperators["operator%="] = "imod";

    // Bitwise operators
    m_pythonOperators["operator&"] = "and";
    m_pythonOperators["operator^"] = "xor";
    m_pythonOperators["operator|"] = "or";
    m_pythonOperators["operator<<"] = "lshift";
    m_pythonOperators["operator>>"] = "rshift";
    m_pythonOperators["operator~"] = "invert";

    // Inplace bitwise operators
    m_pythonOperators["operator&="] = "iand";
    m_pythonOperators["operator^="] = "ixor";
    m_pythonOperators["operator|="] = "ior";
    m_pythonOperators["operator<<="] = "ilshift";
    m_pythonOperators["operator>>="] = "irshift";

    // Comparison operators
    m_pythonOperators["operator=="] = "eq";
    m_pythonOperators["operator!="] = "ne";
    m_pythonOperators["operator<"] = "lt";
    m_pythonOperators["operator>"] = "gt";
    m_pythonOperators["operator<="] = "le";
    m_pythonOperators["operator>="] = "ge";

    // Initialize format units for C++->Python->C++ conversion
    m_formatUnits.clear();
    m_formatUnits.insert("char", "b");
    m_formatUnits.insert("unsigned char", "B");
    m_formatUnits.insert("int", "i");
    m_formatUnits.insert("unsigned int", "I");
    m_formatUnits.insert("short", "h");
    m_formatUnits.insert("unsigned short", "H");
    m_formatUnits.insert("long", "l");
    m_formatUnits.insert("unsigned long", "k");
    m_formatUnits.insert("long long", "L");
    m_formatUnits.insert("__int64", "L");
    m_formatUnits.insert("unsigned long long", "K");
    m_formatUnits.insert("unsigned __int64", "K");
    m_formatUnits.insert("double", "d");
    m_formatUnits.insert("float", "f");
}

FunctionModificationList ShibokenGenerator::functionModifications(const AbstractMetaFunction* func)
{
    FunctionModificationList mods;
    const AbstractMetaClass *cls = func->ownerClass();
    while (cls) {
        mods += func->modifications(cls);
        if (cls == cls->baseClass())
            break;
        cls = cls->baseClass();
    }
    return mods;
}

QString ShibokenGenerator::translateTypeForWrapperMethod(const AbstractMetaType* cType,
                                                         const AbstractMetaClass* context) const
{
    QString result;
    const TypeEntry* tentry = cType->typeEntry();

    if (tentry->isValue() || tentry->isObject() || (cType->isReference() && !cType->isContainer())) {
        result = tentry->qualifiedCppName();
        if (cType->isReference())
            result.append('&');
        else if (tentry->isObject() || cType->isValuePointer())
            result.append('*');
    } else if (cType->isArray()) {
        result = translateTypeForWrapperMethod(cType->arrayElementType(), context) + "[]";
    } else {
        result = translateType(cType, context);
    }

    return result;
}

bool ShibokenGenerator::shouldGenerateCppWrapper(const AbstractMetaClass* metaClass)
{
    bool result = metaClass->isPolymorphic() || metaClass->hasVirtualDestructor();
#ifdef AVOID_PROTECTED_HACK
    result = result || metaClass->hasProtectedFunctions() || metaClass->hasProtectedDestructor();
#endif
    return result && !metaClass->isNamespace() && !metaClass->hasPrivateDestructor();
}

QString ShibokenGenerator::wrapperName(const AbstractMetaClass* metaClass)
{
    if (shouldGenerateCppWrapper(metaClass)) {
        QString result = metaClass->name();
        if (metaClass->enclosingClass()) // is a inner class
            result.replace("::", "_");

        result +="Wrapper";
        return result;
    } else {
        return metaClass->qualifiedCppName();
    }
}

QString ShibokenGenerator::cpythonFunctionName(const AbstractMetaFunction* func)
{
    QString result;

    if (func->ownerClass()) {
        result = cpythonBaseName(func->ownerClass()->typeEntry());
        result += '_';
        if (func->isConstructor() || func->isCopyConstructor())
            result += "New";
        else if (func->isOperatorOverload())
            result += ShibokenGenerator::pythonOperatorFunctionName(func);
        else
            result += func->name();
    } else {
        result = "Sbk" + moduleName() + "Module_" + func->name();
    }

    return result;
}

static QString cpythonEnumFlagsName(QString moduleName, QString qualifiedCppName)
{
    QString result = QString("Sbk%1_%2").arg(moduleName).arg(qualifiedCppName);
    result.replace("::", "_");
    return result;
}

QString ShibokenGenerator::cpythonEnumName(const EnumTypeEntry* enumEntry)
{
    return cpythonEnumFlagsName(moduleName(), enumEntry->qualifiedCppName());
}

QString ShibokenGenerator::cpythonFlagsName(const FlagsTypeEntry* flagsEntry)
{
    return cpythonEnumFlagsName(moduleName(), flagsEntry->originalName());
}

QString ShibokenGenerator::cpythonSpecialCastFunctionName(const AbstractMetaClass* metaClass)
{
    return cpythonBaseName(metaClass->typeEntry())+"SpecialCastFunction";
}

QString ShibokenGenerator::cpythonWrapperCPtr(const AbstractMetaClass* metaClass, QString argName)
{
    return cpythonWrapperCPtr(metaClass->typeEntry(), argName);
}

QString ShibokenGenerator::cpythonWrapperCPtr(const AbstractMetaType* metaType, QString argName)
{
    return cpythonWrapperCPtr(metaType->typeEntry(), argName);
}

QString ShibokenGenerator::cpythonWrapperCPtr(const TypeEntry* type, QString argName)
{
    if (type->isValue() || type->isObject())
        return QString("%1_cptr(%2)").arg(cpythonBaseName(type)).arg(argName);
    return QString();
}

QString ShibokenGenerator::getFunctionReturnType(const AbstractMetaFunction* func, Options options) const
{
    if (func->ownerClass() && (func->isConstructor() || func->isCopyConstructor()))
        return func->ownerClass()->qualifiedCppName() + '*';

    return translateTypeForWrapperMethod(func->type(), func->implementingClass());

    //TODO: check these lines
    //QString modifiedReturnType = QString(func->typeReplaced(0));
    //return modifiedReturnType.isNull() ?
    //translateType(func->type(), func->implementingClass()) : modifiedReturnType;
}

static QString baseConversionString(QString typeName)
{
    return QString("Shiboken::Converter<%1 >::").arg(typeName);
}

void ShibokenGenerator::writeBaseConversion(QTextStream& s, const TypeEntry* type)
{
    s << baseConversionString(type->name());
}

void ShibokenGenerator::writeBaseConversion(QTextStream& s, const AbstractMetaType* type,
                                            const AbstractMetaClass* context)
{
    QString typeName;
    if (type->isPrimitive()) {
        const PrimitiveTypeEntry* ptype = (const PrimitiveTypeEntry*) type->typeEntry();
        if (ptype->basicAliasedTypeEntry())
            ptype = ptype->basicAliasedTypeEntry();
        typeName = ptype->name();
    } else {
        typeName = translateTypeForWrapperMethod(type, context);
    }

    const TypeEntry* tentry = type->typeEntry();

    // If the type is an Object (and a pointer) remove its constness since it
    // is already declared as const in the signature of the generated converter.
    if (tentry->isObject() && typeName.startsWith("const "))
        typeName.remove(0, sizeof("const ") / sizeof(char) - 1);

    // Remove the constness, if any
    if (typeName.startsWith("const ") && type->name() != "char")
        typeName.remove(0, sizeof("const ") / sizeof(char) - 1);

    if (typeName.endsWith("&") && (tentry->isPrimitive() || tentry->isContainer()))
        typeName.chop(1);

    s << baseConversionString(typeName);
}

void ShibokenGenerator::writeToPythonConversion(QTextStream& s, const AbstractMetaType* type,
                                                const AbstractMetaClass* context, QString argumentName)
{
    if (!type)
        return;

    writeBaseConversion(s, type, context);
    s << "toPython";

    if (!argumentName.isEmpty())
        s << '(' << argumentName << ')';
}

void ShibokenGenerator::writeToCppConversion(QTextStream& s, const AbstractMetaType* type,
                                             const AbstractMetaClass* context, QString argumentName)
{
    writeBaseConversion(s, type, context);
    s << "toCpp(" << argumentName << ')';
}

QString ShibokenGenerator::getFormatUnitString(const AbstractMetaFunction* func) const
{
    QString result;
    foreach (const AbstractMetaArgument* arg, func->arguments()) {
        if (func->argumentRemoved(arg->argumentIndex() + 1))
            continue;

        if (arg->type()->isQObject()
            || arg->type()->isObject()
            || arg->type()->isValue()
            || arg->type()->isValuePointer()
            || arg->type()->isReference()) {
            result += 'O';
        } else if (arg->type()->isPrimitive()) {
            const PrimitiveTypeEntry* ptype = (const PrimitiveTypeEntry*) arg->type()->typeEntry();
            if (ptype->basicAliasedTypeEntry())
                ptype = ptype->basicAliasedTypeEntry();
            if (m_formatUnits.contains(ptype->name()))
                result += m_formatUnits[ptype->name()];
            else
                result += 'O';
        } else if (arg->type()->isNativePointer() && arg->type()->name() == "char") {
            result += 'z';
        } else {
            result += 'Y';
        }
    }
    return result;
}

QString ShibokenGenerator::cpythonBaseName(const AbstractMetaType* type)
{
    if (type->name() == "char" && type->isNativePointer())
        return QString("PyString");
    return cpythonBaseName(type->typeEntry());
}

QString ShibokenGenerator::cpythonBaseName(const TypeEntry* type)
{
    QString baseName;
    if ((type->isObject() || type->isValue() || type->isNamespace())) { // && !type->isReference()) {
        baseName = QString("Sbk") + type->name();
    } else if (type->isPrimitive()) {
        const PrimitiveTypeEntry* ptype = (const PrimitiveTypeEntry*) type;
        if (ptype->basicAliasedTypeEntry())
            ptype = ptype->basicAliasedTypeEntry();
        if (ptype->targetLangApiName() == ptype->name())
            baseName = m_pythonPrimitiveTypeName[ptype->name()];
        else
            baseName = ptype->targetLangApiName();
    } else if (type->isEnum()) {
        baseName = cpythonEnumName((const EnumTypeEntry*) type);
    } else if (type->isFlags()) {
        baseName = cpythonFlagsName((const FlagsTypeEntry*) type);
    } else if (type->isContainer()) {
        const ContainerTypeEntry* ctype = (const ContainerTypeEntry*) type;
        switch (ctype->type()) {
            case ContainerTypeEntry::ListContainer:
            case ContainerTypeEntry::StringListContainer:
            case ContainerTypeEntry::LinkedListContainer:
            case ContainerTypeEntry::VectorContainer:
            case ContainerTypeEntry::StackContainer:
            case ContainerTypeEntry::QueueContainer:
                //baseName = "PyList";
                //break;
            case ContainerTypeEntry::PairContainer:
                //baseName = "PyTuple";
                baseName = "PySequence";
                break;
            case ContainerTypeEntry::SetContainer:
                baseName = "PySet";
                break;
            case ContainerTypeEntry::MapContainer:
            case ContainerTypeEntry::MultiMapContainer:
            case ContainerTypeEntry::HashContainer:
            case ContainerTypeEntry::MultiHashContainer:
                baseName = "PyDict";
                break;
            default:
                Q_ASSERT(false);
        }
    } else {
        baseName = "PyObject";
    }
    return baseName.replace("::", "_");
}

QString ShibokenGenerator::cpythonTypeName(const AbstractMetaClass* metaClass)
{
    return cpythonTypeName(metaClass->typeEntry());
}

QString ShibokenGenerator::cpythonTypeName(const TypeEntry* type)
{
    return cpythonBaseName(type) + "_Type";
}

QString ShibokenGenerator::cpythonOperatorFunctionName(const AbstractMetaFunction* func)
{
    if (!func->isOperatorOverload())
        return QString();
    return QString("Sbk") + func->ownerClass()->name()
            + '_' + pythonOperatorFunctionName(func->originalName());
}

QString ShibokenGenerator::pythonPrimitiveTypeName(QString cppTypeName)
{
    return ShibokenGenerator::m_pythonPrimitiveTypeName.value(cppTypeName, QString());
}

QString ShibokenGenerator::pythonPrimitiveTypeName(const PrimitiveTypeEntry* type)
{
    if (type->basicAliasedTypeEntry())
        type = type->basicAliasedTypeEntry();
    return pythonPrimitiveTypeName(type->name());
}

QString ShibokenGenerator::pythonOperatorFunctionName(QString cppOpFuncName)
{
    QString value = m_pythonOperators.value(cppOpFuncName);
    if (value.isEmpty()) {
        ReportHandler::warning("Unknown operator: "+cppOpFuncName);
        value = "UNKNOWN_OPERATOR";
    }
    value.prepend("__").append("__");
    return value;
}

QString ShibokenGenerator::pythonOperatorFunctionName(const AbstractMetaFunction* func)
{
    QString op = pythonOperatorFunctionName(func->originalName());
    if (func->arguments().isEmpty()) {
        if (op == "__sub__")
            op = QString("__neg__");
        else if (op == "__add__")
            op = QString("__pos__");
    } else if (func->isStatic() && func->arguments().size() == 2) {
        // If a operator overload function has 2 arguments and
        // is static we assume that it is a reverse operator.
        op = op.insert(2, 'r');
    }
    return op;
}

QString ShibokenGenerator::pythonRichCompareOperatorId(QString cppOpFuncName)
{
    return QString("Py_%1").arg(m_pythonOperators.value(cppOpFuncName).toUpper());
}

QString ShibokenGenerator::pythonRichCompareOperatorId(const AbstractMetaFunction* func)
{
    return pythonRichCompareOperatorId(func->originalName());
}

bool ShibokenGenerator::isNumber(QString cpythonApiName)
{
    return cpythonApiName == "PyInt"
            || cpythonApiName == "PyFloat"
            || cpythonApiName == "PyLong";
}

bool ShibokenGenerator::isNumber(const TypeEntry* type)
{
    if (!type->isPrimitive())
        return false;
    return isNumber(pythonPrimitiveTypeName((const PrimitiveTypeEntry*) type));
}

bool ShibokenGenerator::isNumber(const AbstractMetaType* type)
{
    return isNumber(type->typeEntry());
}

bool ShibokenGenerator::isPyInt(const TypeEntry* type)
{
    if (!type->isPrimitive())
        return false;
    return pythonPrimitiveTypeName((const PrimitiveTypeEntry*) type) == "PyInt";
}

bool ShibokenGenerator::isPyInt(const AbstractMetaType* type)
{
    return isPyInt(type->typeEntry());
}

bool ShibokenGenerator::shouldDereferenceArgumentPointer(const AbstractMetaArgument* arg)
{
    const AbstractMetaType* argType = arg->type();
    const TypeEntry* type = argType->typeEntry();
    return (type->isValue() || type->isObject()) && (argType->isValue() || argType->isReference());
}

static QString checkFunctionName(QString baseName, bool genericNumberType, bool checkExact)
{
    return QString("%1_Check%2")
           .arg((genericNumberType && ShibokenGenerator::isNumber(baseName) ? "PyNumber" : baseName))
           .arg((checkExact && !genericNumberType ? "Exact" : ""));
}

QString ShibokenGenerator::cpythonCheckFunction(const AbstractMetaType* metaType, bool genericNumberType, bool checkExact)
{
    if (metaType->typeEntry()->isCustom())
        return guessCPythonCheckFunction(metaType->typeEntry()->name());
    return checkFunctionName(cpythonBaseName(metaType), genericNumberType, checkExact);
}

QString ShibokenGenerator::cpythonCheckFunction(const TypeEntry* type, bool genericNumberType, bool checkExact)
{
    if (type->isCustom())
        return guessCPythonCheckFunction(type->name());
    return checkFunctionName(cpythonBaseName(type), genericNumberType, checkExact);
}

QString ShibokenGenerator::guessCPythonCheckFunction(const QString& type)
{
    if (type == "PyTypeObject")
        return "PyType_Check";
    return type+"_Check";
}

QString ShibokenGenerator::cpythonIsConvertibleFunction(const TypeEntry* type)
{
    QString baseName;
    QTextStream s(&baseName);
    writeBaseConversion(s, type);
    s << "isConvertible";
    s.flush();
    return baseName;
}

QString ShibokenGenerator::cpythonIsConvertibleFunction(const AbstractMetaType* metaType)
{
    QString baseName;
    QTextStream s(&baseName);
    if (metaType->isValuePointer() || metaType->typeEntry()->isObject()) {
        const AbstractMetaClass* context = classes().findClass(metaType->typeEntry()->name());
        writeBaseConversion(s, metaType, context);
    } else {
        writeBaseConversion(s, metaType->typeEntry());
    }
    s << "isConvertible";
    s.flush();
    return baseName;
}

QString ShibokenGenerator::argumentString(const AbstractMetaFunction *func,
                                          const AbstractMetaArgument *argument,
                                          Options options) const
{
    QString modified_type = func->typeReplaced(argument->argumentIndex() + 1);
    QString arg;

    if (modified_type.isEmpty())
        arg = translateType(argument->type(), func->implementingClass(), options);
    else
        arg = modified_type.replace('$', '.');

    if (!(options & Generator::SkipName)) {
        arg += " ";
        arg += argument->argumentName();
    }

    QList<ReferenceCount> referenceCounts;
    referenceCounts = func->referenceCounts(func->implementingClass(), argument->argumentIndex() + 1);
    if ((options & Generator::SkipDefaultValues) != Generator::SkipDefaultValues &&
        !argument->originalDefaultValueExpression().isEmpty())
    {
        QString default_value = argument->originalDefaultValueExpression();
        if (default_value == "NULL")
            default_value = NULL_VALUE;

        //WORKAROUND: fix this please
        if (default_value.startsWith("new "))
            default_value.remove(0, 4);

        arg += " = " + default_value;
    }

    return arg;
}

void ShibokenGenerator::writeArgument(QTextStream &s,
                                      const AbstractMetaFunction *func,
                                      const AbstractMetaArgument *argument,
                                      Options options) const
{
    s << argumentString(func, argument, options);
}

void ShibokenGenerator::writeFunctionArguments(QTextStream &s,
                                               const AbstractMetaFunction *func,
                                               Options options) const
{
    AbstractMetaArgumentList arguments = func->arguments();

    if (options & Generator::WriteSelf) {
        s << func->implementingClass()->name() << '&';
        if (!(options & SkipName))
            s << " self";
    }

    int argUsed = 0;
    for (int i = 0; i < arguments.size(); ++i) {
        if ((options & Generator::SkipRemovedArguments) && func->argumentRemoved(i+1))
            continue;

        if ((options & Generator::WriteSelf) || argUsed != 0)
            s << ", ";
        writeArgument(s, func, arguments[i], options);
        argUsed++;
    }
}

QString ShibokenGenerator::functionReturnType(const AbstractMetaFunction* func, Options options) const
{
    QString modifiedReturnType = QString(func->typeReplaced(0));
    if (!modifiedReturnType.isNull() && !(options & OriginalTypeDescription))
        return modifiedReturnType;
    else
        return translateType(func->type(), func->implementingClass(), options);
}

QString ShibokenGenerator::functionSignature(const AbstractMetaFunction *func,
                                             QString prepend,
                                             QString append,
                                             Options options,
                                             int argCount) const
{
    QString result;
    QTextStream s(&result);
    // The actual function
    if (!(func->isEmptyFunction() ||
          func->isNormal() ||
          func->isSignal())) {
        options |= Generator::SkipReturnType;
    } else {
        s << functionReturnType(func, options) << ' ';
    }

    // name
    QString name(func->originalName());
    if (func->isConstructor())
        name = wrapperName(func->ownerClass());

    s << prepend << name << append << '(';
    writeFunctionArguments(s, func, options);
    s << ')';

    if (func->isConstant() && !(options & Generator::ExcludeMethodConst))
        s << " const";

    return result;
}

bool ShibokenGenerator::hasInjectedCodeOrSignatureModification(const AbstractMetaFunction* func)
{
    foreach (FunctionModification mod, functionModifications(func)) {
        if (mod.isCodeInjection() || mod.isRenameModifier())
            return true;
    }
    return false;
}

void ShibokenGenerator::writeArgumentNames(QTextStream &s,
                                           const AbstractMetaFunction *func,
                                           Options options) const
{
    AbstractMetaArgumentList arguments = func->arguments();
    int argCount = 0;
    for (int j = 0, max = arguments.size(); j < max; j++) {

        if ((options & Generator::SkipRemovedArguments) &&
            (func->argumentRemoved(arguments.at(j)->argumentIndex() +1)))
            continue;

        if (argCount > 0)
            s << ", ";

        QString argName;

        if ((options & Generator::BoxedPrimitive) &&
            !arguments.at(j)->type()->isReference() &&
            (arguments.at(j)->type()->isQObject() ||
             arguments.at(j)->type()->isObject())) {
            //s << "brian::wrapper_manager::instance()->retrieve( " << arguments.at(j)->argumentName() << " )";
            // TODO: replace boost thing
            s << "python::ptr( " << arguments.at(j)->argumentName() << " )";
        } else {
            s << arguments.at(j)->argumentName();
        }
        argCount++;
    }
}

AbstractMetaFunctionList ShibokenGenerator::queryGlobalOperators(const AbstractMetaClass *metaClass)
{
    AbstractMetaFunctionList result;

    foreach (AbstractMetaFunction *func, metaClass->functions()) {
        if (func->isInGlobalScope() && func->isOperatorOverload())
            result.append(func);
    }
    return result;
}

AbstractMetaFunctionList ShibokenGenerator::queryFunctions(const AbstractMetaClass *metaClass, bool allFunctions)
{
    AbstractMetaFunctionList result;

    if (allFunctions) {
        int defaultFlags = AbstractMetaClass::NormalFunctions | AbstractMetaClass::Visible;
        defaultFlags |= metaClass->isInterface() ? 0 : AbstractMetaClass::ClassImplements;

        // Constructors
        result = metaClass->queryFunctions(AbstractMetaClass::Constructors
                                           | defaultFlags);

        // put enum constructor first to avoid conflict with int contructor
        result = sortConstructor(result);

        // Final functions
        result += metaClass->queryFunctions(AbstractMetaClass::FinalInTargetLangFunctions
                                            | AbstractMetaClass::NonStaticFunctions
                                            | defaultFlags);

        //virtual
        result += metaClass->queryFunctions(AbstractMetaClass::VirtualInTargetLangFunctions
                                            | AbstractMetaClass::NonStaticFunctions
                                            | defaultFlags);

        // Static functions
        result += metaClass->queryFunctions(AbstractMetaClass::StaticFunctions | defaultFlags);

        // Empty, private functions, since they aren't caught by the other ones
        result += metaClass->queryFunctions(AbstractMetaClass::Empty
                                            | AbstractMetaClass::Invisible
                                            | defaultFlags);
        // Signals
        result += metaClass->queryFunctions(AbstractMetaClass::Signals | defaultFlags);
    } else {
        result = metaClass->functionsInTargetLang();
    }

    return result;
}

void ShibokenGenerator::writeFunctionCall(QTextStream& s,
                                          const AbstractMetaFunction* func,
                                          Options options) const
{
    if (!(options & Generator::SkipName))
        s << (func->isConstructor() ? func->ownerClass()->qualifiedCppName() : func->originalName());
    s << '(';
    writeArgumentNames(s, func, options);
    s << ')';
}

AbstractMetaFunctionList ShibokenGenerator::filterFunctions(const AbstractMetaClass* metaClass)
{
    AbstractMetaFunctionList lst = queryFunctions(metaClass, true);
    foreach (AbstractMetaFunction *func, lst) {
        //skip signals
        if (func->isSignal()
            || func->isDestructor()
#ifndef AVOID_PROTECTED_HACK
            || (func->isModifiedRemoved() && !func->isAbstract()))
#else
            || (func->isModifiedRemoved() && !func->isAbstract() && !func->isProtected()))
#endif
            lst.removeOne(func);
    }

    //virtual not implemented in current class
    AbstractMetaFunctionList virtualLst = metaClass->queryFunctions(AbstractMetaClass::VirtualFunctions);
    foreach (AbstractMetaFunction* func, virtualLst) {
        if ((func->implementingClass() != metaClass) && !lst.contains(func))
            lst.append(func);
    }

    //append global operators
    lst += queryGlobalOperators(metaClass);

    return lst;
    //return metaClass->functions();
}

CodeSnipList ShibokenGenerator::getCodeSnips(const AbstractMetaFunction *func)
{
    CodeSnipList result;
    const AbstractMetaClass* metaClass = func->implementingClass();
    while (metaClass) {
        foreach (FunctionModification mod, func->modifications(metaClass)) {
            if (mod.isCodeInjection())
                result << mod.snips;
        }

        if (metaClass == metaClass->baseClass())
            break;
        metaClass = metaClass->baseClass();
    }

    return result;
}

void ShibokenGenerator::writeCodeSnips(QTextStream& s,
                                       const CodeSnipList& codeSnips,
                                       CodeSnip::Position position,
                                       TypeSystem::Language language,
                                       const AbstractMetaFunction* func,
                                       const AbstractMetaArgument* lastArg,
                                       const AbstractMetaClass* context)
{
    static QRegExp toPythonRegex("%CONVERTTOPYTHON\\[([^\\[]*)\\]");
    static QRegExp toCppRegex("%CONVERTTOCPP\\[([^\\[]*)\\]");
    static QRegExp pyArgsRegex("%PYARG_(\\d+)");

    // detect is we should use pyargs instead of args as variable name for python arguments
    bool usePyArgs;
    int numArgs;
    if (func) {
        // calc num of real arguments.
        int argsRemoved = 0;
        for (int i = 0; i < func->arguments().size(); i++) {
            if (func->argumentRemoved(i+1))
                argsRemoved++;
        }
        numArgs = func->arguments().size() - argsRemoved;

        usePyArgs = getMinMaxArguments(func).second > 1 || func->isConstructor();
    }

    foreach (CodeSnip snip, codeSnips) {
        if ((position != CodeSnip::Any && snip.position != position) || !(snip.language & language))
            continue;

        QString code;
        QTextStream tmpStream(&code);
        Indentation indent1(INDENT);
        Indentation indent2(INDENT);
        formatCode(tmpStream, snip.code(), INDENT);

        if (context) {
            // replace template variable for the Python Type object for the
            // class context in which the variable is used
            code.replace("%PYTHONTYPEOBJECT", cpythonTypeName(context) + ".pytype");
        }

        // replace "toPython "converters
        code.replace(toPythonRegex, "Shiboken::Converter<\\1 >::toPython");

        // replace "toCpp "converters
        code.replace(toCppRegex, "Shiboken::Converter<\\1 >::toCpp");

        if (func) {
            // replace %PYARG_# variables
            code.replace("%PYARG_0", retvalVariableName());
            if (snip.language == TypeSystem::TargetLangCode) {
                if (numArgs > 1) {
                    code.replace(pyArgsRegex, "pyargs[\\1-1]");
                } else {
                    static QRegExp pyArgsRegexCheck("%PYARG_([2-9]+)");
                    if (pyArgsRegexCheck.indexIn(code) != -1)
                        ReportHandler::warning("Wrong index for %PYARG variable ("+pyArgsRegexCheck.cap(1)+") on "+func->signature());
                    else
                        code.replace("%PYARG_1", usePyArgs ? "pyargs[0]" : "arg");
                }
            } else {
                // Replaces the simplest case of attribution to a Python argument
                // on the binding virtual method.
                static QRegExp pyArgsAttributionRegex("%PYARG_(\\d+)\\s*=[^=]\\s*([^;]+)");
                code.replace(pyArgsAttributionRegex, "PyTuple_SET_ITEM(pyargs, \\1-1, \\2)");

                code.replace(pyArgsRegex, "PyTuple_GET_ITEM(pyargs, \\1-1)");
            }

            // replace %ARG#_TYPE variables
            foreach (const AbstractMetaArgument* arg, func->arguments()) {
                QString argTypeVar = QString("%ARG%1_TYPE").arg(arg->argumentIndex() + 1);
                QString argTypeVal = arg->type()->cppSignature();
                code.replace(argTypeVar, argTypeVal);
            }

            static QRegExp cppArgTypeRegexCheck("%ARG(\\d+)_TYPE");
            int pos = 0;
            while ((pos = cppArgTypeRegexCheck.indexIn(code, pos)) != -1) {
                ReportHandler::warning("Wrong index for %ARG#_TYPE variable ("+cppArgTypeRegexCheck.cap(1)+") on "+func->signature());
                pos += cppArgTypeRegexCheck.matchedLength();
            }

            // replace template variable for return variable name
            if (func->isConstructor()) {
                code.replace("%0.", QString("%1->").arg("cptr"));
                code.replace("%0", "cptr");
            } else if (func->type()) {
                QString pyRetVal = cpythonWrapperCPtr(func->type(), retvalVariableName());
                if (func->type()->typeEntry()->isValue() || func->type()->typeEntry()->isObject())
                    code.replace("%0.", QString("%1->").arg(pyRetVal));
                code.replace("%0", pyRetVal);
            }

            // replace template variable for self Python object
            QString pySelf;
            if (snip.language == TypeSystem::NativeCode)
                pySelf = "pySelf";
            else
                pySelf = "self";
            code.replace("%PYSELF", pySelf);

            // replace template variable for pointer to C++ this object
            if (func->implementingClass()) {
                QString cppSelf;
                if (snip.language == TypeSystem::NativeCode)
                    cppSelf = "this";
                else
                    cppSelf = "cppSelf";
                code.replace("%CPPSELF.", QString("%1->").arg(cppSelf));
                code.replace("%CPPSELF", cppSelf);

                // replace template variable for the Python Type object for the
                // class implementing the method in which the code snip is written
                if (func->isStatic()) {
                    code.replace("%PYTHONTYPEOBJECT", cpythonTypeName(func->implementingClass()) + ".pytype");
                } else {
                    code.replace("%PYTHONTYPEOBJECT.", QString("%1->ob_type->").arg(pySelf));
                    code.replace("%PYTHONTYPEOBJECT", QString("%1->ob_type").arg(pySelf));
                }
            }

            // replace template variables %# for individual arguments
            int removed = 0;
            for (int i = 0; i < func->arguments().size(); i++) {
                const AbstractMetaArgument* arg = func->arguments().at(i);
                QString argReplacement;
                if (snip.language == TypeSystem::TargetLangCode) {
                    if (func->argumentRemoved(i+1)) {
                        if (!arg->defaultValueExpression().isEmpty())
                            argReplacement = arg->defaultValueExpression();
                        removed++;
                    }

                    if (lastArg && arg->argumentIndex() > lastArg->argumentIndex())
                        argReplacement = arg->defaultValueExpression();

                    if (argReplacement.isEmpty()) {
                        argReplacement = QString("cpp_arg%1").arg(i - removed);
                        if (shouldDereferenceArgumentPointer(arg))
                            argReplacement.prepend("(*").append(')');
                    }
                } else {
                    argReplacement = arg->argumentName();
                }
                code.replace("%" + QString::number(i+1), argReplacement);
            }

            // replace template %ARGUMENT_NAMES variable for a list of arguments
            removed = 0;
            QStringList argumentNames;
            foreach (const AbstractMetaArgument* arg, func->arguments()) {
                if (snip.language == TypeSystem::TargetLangCode) {
                    if (func->argumentRemoved(arg->argumentIndex() + 1)) {
                        if (!arg->defaultValueExpression().isEmpty())
                            argumentNames << arg->defaultValueExpression();
                        removed++;
                        continue;
                    }

                    QString argName;
                    if (lastArg && arg->argumentIndex() > lastArg->argumentIndex()) {
                        argName = arg->defaultValueExpression();
                    } else {
                        argName = QString("cpp_arg%1").arg(arg->argumentIndex() - removed);
                        if (shouldDereferenceArgumentPointer(arg))
                            argName.prepend('*');
                    }
                    argumentNames << argName;
                } else {
                    argumentNames << arg->argumentName();
                }
            }
            code.replace("%ARGUMENT_NAMES", argumentNames.join(", "));

            if (snip.language == TypeSystem::NativeCode) {
                // replace template %PYTHON_ARGUMENTS variable for a pointer to the Python tuple
                // containing the converted virtual method arguments received from C++ to be passed
                // to the Python override
                code.replace("%PYTHON_ARGUMENTS", "pyargs");

                // replace variable %PYTHON_METHOD_OVERRIDE for a pointer to the Python method
                // override for the C++ virtual method in which this piece of code was inserted
                code.replace("%PYTHON_METHOD_OVERRIDE", "py_override");
            }

#ifdef AVOID_PROTECTED_HACK
            // If the function being processed was added by the user via type system,
            // Shiboken needs to find out if there are other overloads for the same method
            // name and if any of them is of the protected visibility. This is used to replace
            // calls to %FUNCTION_NAME on user written custom code for calls to the protected
            // dispatcher.
            bool hasProtectedOverload = false;
            if (func->isUserAdded()) {
                foreach (const AbstractMetaFunction* f, getFunctionOverloads(func->ownerClass(), func->name()))
                    hasProtectedOverload |= f->isProtected();
            }

            if (func->isProtected() || hasProtectedOverload) {
                code.replace("%TYPE::%FUNCTION_NAME",
                             QString("%1::%2_protected")
                             .arg(wrapperName(func->ownerClass()))
                             .arg(func->originalName()));
                code.replace("%FUNCTION_NAME", QString("%1_protected").arg(func->originalName()));
            }
#endif

            if (func->isConstructor() && shouldGenerateCppWrapper(func->ownerClass()))
                code.replace("%TYPE", wrapperName(func->ownerClass()));

            replaceTemplateVariables(code, func);
        }

        s << code;
    }
}

bool ShibokenGenerator::injectedCodeUsesCppSelf(const AbstractMetaFunction* func)
{
    CodeSnipList snips = func->injectedCodeSnips(CodeSnip::Any, TypeSystem::TargetLangCode);
    foreach (CodeSnip snip, snips) {
        if (snip.code().contains("%CPPSELF"))
            return true;
    }
    return false;
}

bool ShibokenGenerator::injectedCodeUsesPySelf(const AbstractMetaFunction* func)
{
    CodeSnipList snips = func->injectedCodeSnips(CodeSnip::Any, TypeSystem::NativeCode);
    foreach (CodeSnip snip, snips) {
        if (snip.code().contains("%PYSELF"))
            return true;
    }
    return false;
}

bool ShibokenGenerator::injectedCodeCallsCppFunction(const AbstractMetaFunction* func)
{
    QString funcCall = QString("%1(").arg(func->originalName());
    QString wrappedCtorCall;
    if (func->isConstructor()) {
        funcCall.prepend("new ");
        wrappedCtorCall = QString("new %1(").arg(wrapperName(func->ownerClass()));
    }
    CodeSnipList snips = func->injectedCodeSnips(CodeSnip::Any, TypeSystem::TargetLangCode);
    foreach (CodeSnip snip, snips) {
        if (snip.code().contains("%FUNCTION_NAME(") || snip.code().contains(funcCall)
            || (func->isConstructor() && func->ownerClass()->isPolymorphic()
                && snip.code().contains(wrappedCtorCall)))
            return true;
    }
    return false;
}

bool ShibokenGenerator::injectedCodeCallsPythonOverride(const AbstractMetaFunction* func)
{
    static QRegExp overrideCallRegexCheck("PyObject_Call\\s*\\(\\s*%PYTHON_METHOD_OVERRIDE\\s*,");
    CodeSnipList snips = func->injectedCodeSnips(CodeSnip::Any, TypeSystem::NativeCode);
    foreach (CodeSnip snip, snips) {
        if (overrideCallRegexCheck.indexIn(snip.code()) != -1)
            return true;
    }
    return false;
}

bool ShibokenGenerator::injectedCodeHasReturnValueAttribution(const AbstractMetaFunction* func)
{
    static QRegExp retValAttributionRegexCheck("%PYARG_0\\s*=[^=]\\s*.+");
    CodeSnipList snips = func->injectedCodeSnips(CodeSnip::Any, TypeSystem::TargetLangCode);
    foreach (CodeSnip snip, snips) {
        if (retValAttributionRegexCheck.indexIn(snip.code()) != -1)
            return true;
    }
    return false;
}

bool ShibokenGenerator::hasMultipleInheritanceInAncestry(const AbstractMetaClass* metaClass)
{
    if (!metaClass || metaClass->baseClassNames().isEmpty())
        return false;
    if (metaClass->baseClassNames().size() > 1)
        return true;
    return hasMultipleInheritanceInAncestry(metaClass->baseClass());
}

AbstractMetaClassList ShibokenGenerator::getBaseClasses(const AbstractMetaClass* metaClass)
{
    AbstractMetaClassList baseClasses;
    foreach (QString parent, metaClass->baseClassNames())
        baseClasses << classes().findClass(parent);
    return baseClasses;
}

const AbstractMetaClass* ShibokenGenerator::getMultipleInheritingClass(const AbstractMetaClass* metaClass)
{
    if (!metaClass || metaClass->baseClassNames().isEmpty())
        return 0;
    if (metaClass->baseClassNames().size() > 1)
        return metaClass;
    return getMultipleInheritingClass(metaClass->baseClass());
}

QString ShibokenGenerator::getApiExportMacro() const
{
    return "SHIBOKEN_"+moduleName().toUpper()+"_API"; // a longer name to avoid name clashes
}

QString ShibokenGenerator::getModuleHeaderFileName(QString modName) const
{
    if (modName.isEmpty())
        modName = moduleName();
    return QString("%1_python.h").arg(modName.toLower());
}

/*
static void dumpFunction(AbstractMetaFunctionList lst)
{
    qDebug() << "DUMP FUNCTIONS: ";
    foreach (AbstractMetaFunction *func, lst)
        qDebug() << "*" << func->ownerClass()->name()
                        << func->signature()
                        << "Private: " << func->isPrivate()
                        << "Empty: " << func->isEmptyFunction()
                        << "Static:" << func->isStatic()
                        << "Signal:" << func->isSignal()
                        << "ClassImplements: " <<  (func->ownerClass() != func->implementingClass())
                        << "is operator:" << func->isOperatorOverload()
                        << "is global:" << func->isInGlobalScope();
}
*/

static bool isGroupable(const AbstractMetaFunction* func)
{
    if (func->isSignal() || func->isDestructor() || (func->isModifiedRemoved() && !func->isAbstract()))
        return false;
    // weird operator overloads
    if (func->name() == "operator[]" || func->name() == "operator->")  // FIXME: what about cast operators?
        return false;;
    return true;
}

QMap< QString, AbstractMetaFunctionList > ShibokenGenerator::getFunctionGroups(const AbstractMetaClass* scope)
{
    AbstractMetaFunctionList lst = scope ? scope->functions() : globalFunctions();

    QMap<QString, AbstractMetaFunctionList> results;
    foreach (AbstractMetaFunction* func, lst) {
        if (isGroupable(func))
            results[func->name()].append(func);
    }
    return results;
}

AbstractMetaFunctionList ShibokenGenerator::getFunctionOverloads(const AbstractMetaClass* scope, const QString& functionName)
{
    AbstractMetaFunctionList lst = scope ? scope->functions() : globalFunctions();

    AbstractMetaFunctionList results;
    foreach (AbstractMetaFunction* func, lst) {
        if (func->name() != functionName)
            continue;
        if (isGroupable(func))
            results << func;
    }
    return results;

}

QPair< int, int > ShibokenGenerator::getMinMaxArguments(const AbstractMetaFunction* metaFunction)
{
    AbstractMetaFunctionList overloads = getFunctionOverloads(metaFunction->ownerClass(), metaFunction->name());

    int minArgs = std::numeric_limits<int>::max();
    int maxArgs = 0;
    foreach (const AbstractMetaFunction* func, overloads) {
        int numArgs = 0;
        foreach (const AbstractMetaArgument* arg, func->arguments()) {
            if (!func->argumentRemoved(arg->argumentIndex() + 1))
                numArgs++;
        }
        maxArgs = std::max(maxArgs, numArgs);
        minArgs = std::min(minArgs, numArgs);
    }
    return qMakePair(minArgs, maxArgs);
}

QMap<QString, QString> ShibokenGenerator::options() const
{
    QMap<QString, QString> opts(Generator::options());
    opts.insert(PARENT_CTOR_HEURISTIC, "Enable heuristics to detect parent relationship on constructors.");
    return opts;
}

bool ShibokenGenerator::doSetup(const QMap<QString, QString>& args)
{
    m_useCtorHeuristic = args.contains(PARENT_CTOR_HEURISTIC);
    return true;
}

bool ShibokenGenerator::useCtorHeuristic() const
{
    return m_useCtorHeuristic;
}
