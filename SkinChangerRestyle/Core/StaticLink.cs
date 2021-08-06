namespace SkinChangerRestyle.Core
{
    using SkinChangerRestyle.Core;
    using System;
    using System.Collections.Generic;
    using System.Runtime.InteropServices;

    internal static class StaticLink
    {
        private static List<RegisteredObject> registeredObjects;

        static StaticLink()
        {
            registeredObjects = new List<RegisteredObject>();
        }

        public static bool GetObjectByTag<T>(string tag, out T container)
        {
            try
            {
                foreach (var obj in registeredObjects)
                {
                    if (obj.ObjectTag == tag)
                    {
                        container = (T)obj.IncapsulatedObject;
                        return true;
                    }
                }
            }
            catch
            {
                throw new InvalidCastException($"Object with tag {tag} can not be interpreted as {typeof(T)}");
            }
            container = default;
            return false;
        }

        public static void RegisterObject(string tag, object obj)
        {
            registeredObjects.Add(new RegisteredObject(tag, obj));
        }


        [StructLayout(LayoutKind.Sequential)]
        private struct RegisteredObject
        {
            internal object IncapsulatedObject;
            internal Type ObjectType;
            internal string ObjectTag;

            public RegisteredObject(string tag, object obj)
            {
                IncapsulatedObject = obj;
                ObjectType = obj.GetType();
                ObjectTag = tag;
            }
        }
    }
}
