package main

import (
	"fmt"
	"log"
	"os"
	"strings"
)

type ImportId int
type RequireCall struct {
	FilePosition int // position in the file where the require() call starts (including require)
	TotalLength  int // length of the require call, including the function name and brackets
	Target       string
	TargetId     ImportId
}
type File struct {
	Path     string
	ImportID ImportId
	Imports  []RequireCall
}

func findNext(text string, start int, target string) (position int) {
	return strings.Index(text[start:], target)
}

func ignorePath(target string) bool {
	target = strings.ToLower(target)
	return strings.HasPrefix(target, "http://") || strings.HasPrefix(target, "https://")
}

func findRequireCalls(text string) (calls []RequireCall, err error) {
	// the broadest possible check to discard files without imports asap
	if !strings.Contains(text, "require") {
		return
	}

	// TODO: does not ignore strings or comments

	textPosition := 0
	for {
		remainingString := text[textPosition:]

		search := "require("
		found := strings.Index(remainingString, search)
		if found == -1 {
			break
		}

		callStartPosition := textPosition + found
		callEndPosition := findNext(text, callStartPosition, ")")
		if callEndPosition == -1 {
			err = fmt.Errorf("did not find closing bracket for require call that starts at char %v", callStartPosition)
			return
		}
		callEndPosition += callStartPosition

		calls = append(calls, RequireCall{
			FilePosition: callStartPosition,
			TotalLength:  callEndPosition - callStartPosition,
			Target:       text[callStartPosition+9 : callEndPosition-1],
		})
		textPosition = callEndPosition
	}

	return
}

func readFile(path string) (contents string, err error) {
	bytes, err := os.ReadFile(path)
	if err != nil {
		err = fmt.Errorf("reading file: %w", err)
		return
	}

	contents = string(bytes)
	return
}

func getFiles(entry string) (files []File, err error) {
	foundPaths := map[string]ImportId{}
	nextId := ImportId(0)

	foundPaths[entry] = nextId
	nextId++

	pathsToScan := []string{entry}
	for len(pathsToScan) > 0 {
		// pop
		path := pathsToScan[0]
		pathsToScan = pathsToScan[1:]

		// read file
		var fileContents string
		fileContents, err = readFile(path)
		if err != nil {
			return
		}

		// find require calls
		var calls []RequireCall
		calls, err = findRequireCalls(fileContents)
		if err != nil {
			err = fmt.Errorf("finding require calls: %w", err)
			return
		}

		// loop over every file it required
		for i, requireCall := range calls {
			// ignore if we're already aware of the file
			requirePath := requireCall.Target
			existingId, exists := foundPaths[requirePath]
			if exists {
				calls[i].TargetId = existingId
				continue
			}

			// add required file to queue
			pathsToScan = append(pathsToScan, requirePath)
			foundPaths[requirePath] = nextId
			calls[i].TargetId = nextId
			nextId++
		}

		// TODO: dont like that I have to do this
		importId, exists := foundPaths[path]
		if !exists {
			err = fmt.Errorf("file doesnt have id: %v", path)
			return
		}

		// add to results
		files = append(files, File{
			Path:     path,
			ImportID: importId,
			Imports:  calls,
		})
	}

	return
}

func buildBundle(files []File) (output string, err error) {
	output += "-- DO NOT EDIT; this file has been automatically generated\n"
	output += "local modules = {}\n"

	for i := len(files) - 1; i >= 0; i-- {
		file := files[i]
		output += "\n"
		output += fmt.Sprintf("-- PATH: %v\n", file.Path)
		output += fmt.Sprintf("-- ID: %v\n", file.ImportID)
		output += fmt.Sprintf("modules[%v] = (function()\n", file.ImportID)

		var fileString string
		fileString, err = readFile(file.Path)
		if err != nil {
			return
		}

		for depIndex := len(file.Imports) - 1; depIndex >= 0; depIndex-- {
			dep := file.Imports[depIndex]
			fileString = fileString[:dep.FilePosition] + fmt.Sprintf("modules[%v]", dep.TargetId) + fileString[dep.FilePosition+dep.TotalLength+1:]
		}

		fileString = "\t" + fileString
		fileString = strings.ReplaceAll(fileString, "\n", "\n\t")

		output += fileString
		output += "\nend)()\n"
	}

	return
}

func main() {
	files, err := getFiles("./sample2.lua")
	if err != nil {
		log.Fatalf("getting files: %v", err)
		return
	}

	output, err := buildBundle(files)
	if err != nil {
		log.Fatalf("building bundle: %v", err)
		return
	}

	outputPath := "./output.lua"
	err = os.WriteFile(outputPath, []byte(output), 0666)
	if err != nil {
		log.Fatalf("writing output: %v", err)
	}
	fmt.Printf("wrote output to %v", outputPath)
}
