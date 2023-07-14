using TweakerScripts.Exceptions;
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Text.RegularExpressions;
using TweakerScriptsInterpreter.ZipExtensions;

/*
 * supposed stategy file syntax:
SECTION INSTALL:
BACKUP RadioBrowser.cgr %AS%\engine\SongSelector\
EXTRACT %PACK_ROOT%\Wavebreaker-Package.zip %AS%\engine
END-SECTION
SECTION REMOVE:
DELETE Wavebreaker-Hook.ini %AS%\engine\
DELETE Wavebreaker-Hook.dll %AS%\engine\channels\
ROLLBACK %BACKUP_PATH%\RadioBrowser.cgr %AS%\engine\SongSelector\
END-SECTION
 * 
 */
namespace TweakerScripts
{
    public class ScriptParser
    {
        public ScriptParser()
        {
            _instructionExecutionMethods = new Dictionary<ActionTokens.Enum, Func<string, Action>> //TODO: need to invent a new algo to fill this dict without hardcode
            {
                { ActionTokens.Enum.PUT, GetPutAction },
                { ActionTokens.Enum.REPLACE, GetReplaceAction },
                { ActionTokens.Enum.BACKUP, GetBackupAction },
                { ActionTokens.Enum.EXTRACT, GetExtractAction },
                { ActionTokens.Enum.DELETE, GetDeleteAction },
                { ActionTokens.Enum.ROLLBACK, GetRollbackAction }
            };
        }

        public ScriptParser(Dictionary<string, string> placeholders) : this() 
        {
            _placeholders = placeholders;
        }

        public Dictionary<string, string> DefinedCharacters => _placeholders;

        private Dictionary<string, string> _placeholders = new Dictionary<string, string>();
        private List<string> _undesiredChars = new List<string>() { "\t", "\r" };
        private Dictionary<ActionTokens.Enum, Func<string, Action>> _instructionExecutionMethods;

        public Dictionary<string, Script> ParseScriptFromFile(string scriptFile)
        {
            var code = File.ReadAllText(scriptFile);
            return ParseScript(code);
        }

        public Dictionary<string, Script> ParseScript(string code)
        {
            var codeSections = ParseScriptSections(RemoveUndesiredCharactersFromSource(code));
            var result = new Dictionary<string, Script>();

            foreach (var section in codeSections)
            {
                result.Add(section.Title, ParseInstructionSection(section));
            }

            return result;
        }

        private Script ParseInstructionSection(InstructionSection sourceCode)
        {
            var rawCode = sourceCode.Content.Split('\n');
            var script = new Script();
            var lineCounter = 0;

            foreach (var line in rawCode)
            {
                lineCounter++;
                var cl = line.Trim('\n', '\t', ' ');    
                foreach (var avaiableToken in (ActionTokens.Enum[])Enum.GetValues(typeof(ActionTokens.Enum)))
                {
                    if (cl.StartsWith(avaiableToken.ToString()))
                    {
                        var action = _instructionExecutionMethods[avaiableToken]?.Invoke(cl.Substring(avaiableToken.ToString().Length));

                        if (action == null)
                            throw new ScriptParsingException($"Script Parsing failure. Line {lineCounter} - invalid syntax");

                        script.Operations.Add(action);
                    }
                }
            }

            return script;
        }

        private List<InstructionSection> ParseScriptSections(string script)
        {
            var sections = new List<InstructionSection>();
            var sectionPattern = @"SECTION\s+(?<Title>[^\s:]+)\s*:\s*\n(?<Content>.*?)\nEND-SECTION";
            var regex = new Regex(sectionPattern, RegexOptions.Singleline);

            var matches = regex.Matches(script);
            foreach (Match match in matches)
            {
                var title = match.Groups["Title"].Value;
                var content = match.Groups["Content"].Value.Trim();

                var section = new InstructionSection { Title = title, Content = content };
                sections.Add(section);
            }

            return sections;
        }

        private string RemoveUndesiredCharactersFromSource(string source, List<string> undesiredCharacters = null)
        {
            if (undesiredCharacters == null)
                undesiredCharacters = _undesiredChars;

            foreach (var ch in undesiredCharacters)
            {
                source = source.Replace(ch, string.Empty);
            }

            return source;
        }

        private Action GetPutAction(string instruction)
        {
            var instructionParams = SplitAndFormatInstructionParameters(instruction);

            var targetFile = instructionParams[0];
            var destination = instructionParams[1];

            return new Action(() =>
            {
                File.Move(targetFile, destination);
            });
        }

        private Action GetExtractAction(string instruction)
        {
            var instructionParams = SplitAndFormatInstructionParameters(instruction);

            var archivePath = instructionParams[0];
            var destination = instructionParams[1];

            return new Action(() => 
            {
                using (var zip = ZipFile.Open(archivePath, ZipArchiveMode.Read))
                {
                    zip.ExtractForced(destination);
                }
            });
        }

        private Action GetDeleteAction(string instruction)
        {
            var instructionParams = SplitAndFormatInstructionParameters(instruction);

            var target = instructionParams[0];
            var directory = instructionParams[1];
            
            return new Action(() =>
            {
                foreach (var file in Directory.GetFiles(directory))
                {
                    if (Path.GetFileName(file) == target)
                        File.Delete(file);
                }
                return;
            });
        }

        private Action GetReplaceAction(string instruction)
        {
            var instructionParams = SplitAndFormatInstructionParameters(instruction);

            var target = instructionParams[0];
            var newFile = instructionParams[1];

            return new Action(() =>
            {
                File.Copy(target, newFile, true);
            });
        }

        private Action GetBackupAction(string instruction)
        {
            var instructionParams = SplitAndFormatInstructionParameters(instruction);

            var fileName = instructionParams[0];
            var sourceDirectory = instructionParams[1];

            return new Action(() =>
            {
                var files = Directory.GetFiles(sourceDirectory);
                foreach(var file in files)
                {
                    if (Path.GetFileName(file).Equals(fileName))
                    {
                        var fullBackupPath = Path.Combine(_placeholders["%BACKUP_PATH%"], fileName);
                        if (File.Exists(fullBackupPath))
                            File.Delete(fullBackupPath);
                        File.Move(file, Path.Combine(_placeholders["%BACKUP_PATH%"], fileName));
                    }
                }
            });
        }

        private Action GetRollbackAction(string instruction)
        {
            var instructionParams = SplitAndFormatInstructionParameters(instruction);

            var backup = instructionParams[0];
            var destination = instructionParams[1];

            return new Action(() =>
            {
                var backupFile = Path.GetFileName(backup);
                var rootDirectoryInfo = new DirectoryInfo(Path.GetDirectoryName(backup));
                var foundBackups = rootDirectoryInfo.GetFiles(backupFile, SearchOption.AllDirectories);

                if (foundBackups.Length == 0)
                    throw new FileNotFoundException($"Given file {backupFile} not found in backups directory");

                var fullBackupPath = foundBackups[0].FullName;

                File.Copy(Path.Combine(destination, backupFile), fullBackupPath, true);

            });
        }

        private string[] SplitAndFormatInstructionParameters(string instruction)
        {
            var instructionParams = instruction.Trim().Split(' ');

            if (instructionParams.Length != 2)
                throw new ArgumentException($"Server Swapper instruction must take 2 parameters but {instructionParams.Length} given");

            ReplacePlaceholdersEntriesByRealValues(instructionParams);
            return instructionParams;
        }

        private void ReplacePlaceholdersEntriesByRealValues(string[] instructionParams)
        {
            var unresolvedPlaceholders = new List<string>();

            for (var i = 0; i < instructionParams.Length; i++)
            {
                instructionParams[i] = ReplaceFormatPlaceholders(instructionParams[i].Trim(), unresolvedPlaceholders);
            }

            if (unresolvedPlaceholders.Count > 0)
                throw new UnresolvedSymbolException($"Symbols {string.Join(" ", unresolvedPlaceholders)} undefined and can not be resolved");
        }

        private string ReplaceFormatPlaceholders(string instruction, in IList<string> unresolvedPlaceholders)
        {
            var regex = new Regex("%(.*?)%");
            var matchCollection = regex.Matches(instruction);

            foreach (Match match in matchCollection)
            {
                var placeholder = match.Value;

                if (_placeholders.ContainsKey(placeholder))
                    instruction = instruction.Replace(placeholder, _placeholders[placeholder]);
                else
                    unresolvedPlaceholders.Add(placeholder);
            }

            return instruction;
        }


    private static class ActionTokens
        {
            public const string Put = "PUT";
            public const string Delete = "DELETE";
            public const string Replace = "REPLACE";
            public const string Extract = "EXTRACT";
            public const string Rollback = "ROLLBACK";
            public const string Backup = "BACKUP";

            public enum Enum
            {
                PUT,
                DELETE,
                REPLACE,
                EXTRACT,
                ROLLBACK,
                BACKUP
            }
        }

        private static class SectionTokens
        {
            public const string Install = "SECTION INSTALL:";
            public const string Remove = "SECTION REMOVE:";
        }
    }
}
