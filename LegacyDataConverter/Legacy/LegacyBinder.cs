using System;
using System.Reflection;
using System.Runtime.Serialization;
using System.Text.RegularExpressions;

namespace LegacyDataConverter.Legacy
{
    /// <summary>
    /// Redirects deserialization of types originally written by the "ChangerAPI"/"SkinChangerRestyle"
    /// assemblies onto the frozen local copies in this project, regardless of the source assembly's
    /// name/version. This keeps the converter working even after those assemblies evolve or disappear.
    ///
    /// Generic types (e.g. List&lt;NamedBitmap&gt;) embed the assembly-qualified name of their type
    /// arguments directly inside the "typeName" string BinaryFormatter hands us, not just in the
    /// separate "assemblyName" parameter - so a naive "assemblyName == ChangerAPI" check misses them.
    /// Instead we rewrite every embedded "ChangerAPI, Version=..., Culture=..., PublicKeyToken=..."
    /// (and the same for SkinChangerRestyle) reference to point at this assembly before resolving.
    /// </summary>
    internal sealed class LegacyBinder : SerializationBinder
    {
        private static readonly Regex AssemblyQualificationPattern =
            new Regex(@"(ChangerAPI|SkinChangerRestyle), Version=[^,\]]+, Culture=[^,\]]+, PublicKeyToken=[^,\]]+");

        public override Type BindToType(string assemblyName, string typeName)
        {
            var myAssemblyName = Assembly.GetExecutingAssembly().GetName().Name;
            var rewritten = AssemblyQualificationPattern.Replace(typeName, myAssemblyName);

            return Type.GetType(rewritten, throwOnError: false)
                ?? Type.GetType($"{rewritten}, {Assembly.GetExecutingAssembly().FullName}", throwOnError: false);
        }
    }
}
