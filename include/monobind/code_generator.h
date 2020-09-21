// MIT License
// 
// Copyright (c) 2020 #Momo
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <monobind/exception_handling.h>
#include <monobind/mono.h>

#include <ostream>
#include <string>
#include <map>
#include <typeindex>

namespace monobind
{
    template<class T, typename U>
    constexpr std::ptrdiff_t internal_get_member_offset(U T::* member)
    {
        std::aligned_storage_t<sizeof(T)> dummy;
        return (uint8_t*)std::addressof(reinterpret_cast<T*>(&dummy)->*member) - (uint8_t*)std::addressof(dummy);
    }

    class code_generator
    {
        std::map<std::type_index, std::string> m_type_cache;
        std::ostream& m_out;
        mono& m_mono;

        template<typename T>
        const char* get_type_name()
        {
            auto it = m_type_cache.find(typeid(T));
            if (it == m_type_cache.end())
            {
                throw_exception("type was not added to generator before used as a field");
            }
            return it->second.c_str();
        }

    public:
        code_generator(mono& m, std::ostream& out)
            : m_out(out), m_mono(m)
        {
            add_type<char>("byte");
            add_type<wchar_t>("char");
            add_type<float>("float");
            add_type<double>("double");
            add_type<bool>("bool");
            add_type<int8_t>("byte");
            add_type<uint8_t>("byte");
            add_type<int16_t>("short");
            add_type<uint16_t>("ushort");
            add_type<int32_t>("int");
            add_type<uint32_t>("uint");
            add_type<int64_t>("long");
            add_type<uint64_t>("ulong");
            add_type<std::string>("string");
            add_type<std::wstring>("string");

            m_out << "using System;\n";
            m_out << "using System.Runtime.CompilerServices;\n";
            m_out << "using System.Runtime.InteropServices;\n";
            m_out << '\n';
        }

        template<typename T>
        void generate_class_header(const char* name)
        {
            add_type<T>(name);

            m_out << "class " << get_type_name<T>() << '\n';
            m_out << "{\n";
            m_out << "\tprivate IntPtr _nativeHandle;\n";
            m_out << "\tpublic " << name << "(IntPtr nativeHandle) { _nativeHandle = nativeHandle; }\n\n";
        }

        template<typename T>
        void generate_struct_header(const char* name)
        {
            add_type<T>(name);

            m_out << "[StructLayout(LayoutKind.Explicit)]\n";
            m_out << "struct " << get_type_name<T>() << '\n';
            m_out << "{\n";
        }

        void generate_footer()
        {
            m_out << "}\n\n";
        }

        template<typename T, typename X>
        void generate_struct_field(const char* field_name, X T::* f)
        {
            const char* field_type = get_type_name<X>();
            size_t field_offset = internal_get_member_offset(f);

            m_out << "\t[FieldOffset(" << field_offset << ")] ";
            m_out << "public " << field_type << ' ' << field_name << ";\n\n";
        }

        template<typename T, typename X>
        void generate_readonly_struct_field(const char* field_name, X T::* f)
        {
            const char* field_type = get_type_name<X>();
            size_t field_offset = internal_get_member_offset(f);

            m_out << "\t[FieldOffset(" << field_offset << ")] ";
            m_out << "private " << field_type << " _" << field_name << ";\n";
            m_out << "\tpublic " << field_type << ' ' << field_name << " => _" << field_name << ";\n\n";
        }

        template<typename T, typename X>
        void generate_class_field(const char* field_name, X T::* f)
        {
            auto getter = (std::string)"get_" + field_name;
            auto setter = (std::string)"set_" + field_name;

            const char* field_type = get_type_name<X>();
            size_t field_offset = internal_get_member_offset(f);

            m_mono.add_internal_call<X(uintptr_t, size_t)>(getter.c_str(),
                [](uintptr_t ptr, size_t offset) -> X
                {
                    return *reinterpret_cast<X*>((uint8_t*)ptr + offset);
                });

            m_mono.add_internal_call<void(uintptr_t, size_t, X)>(setter.c_str(),
                [](uintptr_t ptr, size_t offset, X x)
                {
                    *reinterpret_cast<X*>((uint8_t*)ptr + offset) = std::move(x);
                });

            m_out << "\t[MethodImpl(MethodImplOptions.InternalCall)]\n";
            m_out << "\tprivate static extern " << field_type << ' ' << getter << "(IntPtr _self, uint _offset);\n";

            m_out << "\t[MethodImpl(MethodImplOptions.InternalCall)]\n";
            m_out << "\tprivate static extern void " << setter << "(IntPtr _self, uint _offset, " << field_type << " _value);\n";

            m_out << "\tpublic " << field_type << ' ' << field_name << " { get => " << getter << "(_nativeHandle, " << field_offset << "); ";
            m_out << "set => " << setter << "(_nativeHandle, " << field_offset << ", value); }\n";

            m_out << '\n';
        }

        template<typename T, typename X>
        void generate_readonly_class_field(const char* field_name, X T::* f)
        {
            auto getter = (std::string)"get_" + field_name;

            const char* field_type = get_type_name<X>();
            size_t field_offset = internal_get_member_offset(f);

            m_mono.add_internal_call<X(uintptr_t, size_t)>(getter.c_str(),
                [](uintptr_t ptr, size_t offset) -> X
                {
                    return *reinterpret_cast<X*>((uint8_t*)ptr + offset);
                });

            m_out << "\t[MethodImpl(MethodImplOptions.InternalCall)]\n";
            m_out << "\tprivate static extern " << field_type << ' ' << getter << "(System.IntPtr _self, uint _offset);\n";

            m_out << "\tpublic " << field_type << ' ' << field_name << " => " << getter << "(_nativeHandle, " << field_offset << ");\n\n";
        }

        template<typename GetCallable, typename SetCallable>
        void generate_class_property(const char* name, GetCallable&& get, SetCallable&& set)
        {
            using ReturnType = decltype(get(uintptr_t()));

            auto getter = (std::string)"get_" + name;
            auto setter = (std::string)"set_" + name;

            const char* field_type = get_type_name<ReturnType>();

            m_mono.add_internal_call<ReturnType(uintptr_t)>(getter.c_str(), std::forward<GetCallable>(get));
            m_mono.add_internal_call<void(uintptr_t, ReturnType)>(setter.c_str(), std::forward<SetCallable>(set));

            m_out << "\t[MethodImpl(MethodImplOptions.InternalCall)]\n";
            m_out << "\tprivate static extern " << field_type << ' ' << getter << "(IntPtr _self);\n";

            m_out << "\t[MethodImpl(MethodImplOptions.InternalCall)]\n";
            m_out << "\tprivate static extern void " << setter << "(IntPtr _self, " << field_type << " _value);\n";

            m_out << "\tpublic " << field_type << ' ' << name << " { get => " << getter << "(_nativeHandle); ";
            m_out << "set => " << setter << "(_nativeHandle, value); }\n";

            m_out << '\n';
        }

        template<typename GetCallable>
        void generate_readonly_class_property(const char* name, GetCallable&& get)
        {
            using ReturnType = decltype(get(uintptr_t()));

            auto getter = (std::string)"get_" + name;

            const char* field_type = get_type_name<ReturnType>();

            m_mono.add_internal_call<ReturnType(uintptr_t)>(getter.c_str(), std::forward<GetCallable>(get));

            m_out << "\t[MethodImpl(MethodImplOptions.InternalCall)]\n";
            m_out << "\tprivate static extern " << field_type << ' ' << getter << "(IntPtr _self);\n";

            m_out << "\tpublic " << field_type << ' ' << name << " => " << getter << "(_nativeHandle); ";
            m_out << '\n';
        }

        template<typename T, typename GetCallable, typename SetCallable>
        void generate_struct_property(const char* name, GetCallable&& get, SetCallable&& set)
        {
            using ReturnType = decltype(get(uintptr_t()));

            auto getter = (std::string)"get_" + name;
            auto setter = (std::string)"set_" + name;

            const char* field_type = get_type_name<ReturnType>();
            const char* struct_type = get_type_name<T>();

            m_mono.add_internal_call<ReturnType(uintptr_t)>(getter.c_str(), std::forward<GetCallable>(get));
            m_mono.add_internal_call<void(uintptr_t, ReturnType)>(setter.c_str(), std::forward<SetCallable>(set));

            m_out << "\t[MethodImpl(MethodImplOptions.InternalCall)]\n";
            m_out << "\tprivate static extern " << field_type << ' ' << getter << "(ref " << struct_type << " _self);\n";

            m_out << "\t[MethodImpl(MethodImplOptions.InternalCall)]\n";
            m_out << "\tprivate static extern void " << setter << "(ref " << struct_type << " _self, " << field_type << " _value);\n";

            m_out << "\tpublic " << field_type << ' ' << name << " { get => " << getter << "(ref this); ";
            m_out << "set => " << setter << "(ref this, value); }\n";

            m_out << '\n';
        }

        template<typename T, typename GetCallable>
        void generate_readonly_struct_property(const char* name, GetCallable&& get)
        {
            using ReturnType = decltype(get(uintptr_t()));

            auto getter = (std::string)"get_" + name;

            const char* field_type = get_type_name<ReturnType>();
            const char* struct_type = get_type_name<T>();

            m_mono.add_internal_call<ReturnType(uintptr_t)>(getter.c_str(), std::forward<GetCallable>(get));

            m_out << "\t[MethodImpl(MethodImplOptions.InternalCall)]\n";
            m_out << "\tprivate static extern " << field_type << ' ' << getter << "(ref " << struct_type << " _self);\n";

            m_out << "\tpublic " << field_type << ' ' << name << " => " << getter << "(ref this); ";
            m_out << '\n';
        }

        template<typename T>
        void add_type(const char* name)
        {
            std::type_index idx = typeid(T);
            if (m_type_cache.find(idx) == m_type_cache.end())
                m_type_cache[idx] = name;
        }
    };
}