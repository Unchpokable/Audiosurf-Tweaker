using NUnit.Framework;
using System.Collections.Generic;
using TweakerScripts;
using TweakerScripts.Exceptions;

namespace TweakerScriptsInterpreter.Tests
{
    [TestFixture]
    public class ScriptParser_Tests
    {
        private static string _defaultScriptToTests = @"
SECTION INSTALL:
BACKUP RadioBrowser.cgr %AS%\engine\SongSelector\
EXTRACT %PACK_ROOT%\Wavebreaker-Package.zip %AS%\engine
END-SECTION
SECTION REMOVE:
DELETE Wavebreaker-Hook.ini %AS%\engine\
DELETE Wavebreaker-Hook.dll %AS%\engine\channels\
ROLLBACK %BACKUP_PATH%\RadioBrowser.cgr %AS%\engine\SongSelector\
END-SECTION
";

        private Dictionary<string, string> _mockDefines = new Dictionary<string, string>
        {
            {"%AS%", "C:\\Program Files (x86)\\steam\\steamapps\\common\\audiosurf\\engine" },
            {"%PACK_ROOT%", "C:\\Users\\Unchp\\OneDrive\\Документы\\My Web Sites" },
            {"%BACKUP_PATH%", "C:\\Users\\Unchp\\OneDrive\\Документы\\My Web Sites" }
        };

        [Test]
        public void TestScriptParserWorks()
        {
            var scriptReader = new ScriptParser(_mockDefines);
            var script = scriptReader.ParseScript(_defaultScriptToTests);
            Assert.That(script.Keys.Count == 2); // 2 sections
            Assert.That(script.ContainsKey("INSTALL"));
            Assert.That(script.ContainsKey("REMOVE"));
            Assert.That(script["INSTALL"].Operations.Count == 2);
            Assert.That(script["REMOVE"].Operations.Count == 3);
        }

        [Test]
        public void TestParserThrowsExceptionWhenUndefinedPlaceholder()
        {
            var scriptReder = new ScriptParser();
            Assert.Throws<UnresolvedSymbolException>(() => scriptReder.ParseScript(_defaultScriptToTests)) ;
        }
    }
}
