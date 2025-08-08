#!/usr/bin/env ruby

require 'pathname'
require 'set'
require 'optparse'
require 'fileutils'

class CppModuleParser
  attr_reader :dependencies, :module_exports, :module_imports, :stats

  def initialize(options = {})
    @dependencies = Hash.new { |h, k| h[k] = Set.new }
    @module_exports = {} # module_name -> source_file
    @module_imports = Hash.new { |h, k| h[k] = Set.new } # file -> imported_modules
    @include_dependencies = Hash.new { |h, k| h[k] = Set.new } # file -> included_files
    @options = {
      verbose: false,
      include_std: false,
      include_headers: true,
      module_ext: '.cppm',
      debug: false,
      src_dir: 'src',      # Source directory
      obj_dir: 'obj'       # Object directory
    }.merge(options)
    @stats = {
      files_processed: 0,
      modules_found: 0,
      imports_found: 0,
      includes_found: 0,
      errors: []
    }
  end

  # Helper method to convert source file path to object file path
  # This handles the mapping from src/ to obj/ directory structure
  def source_to_object_path(source_path)
    # Remove leading ./ if present for cleaner paths
    clean_source = source_path.sub(/^\.\//, '')
    
    puts "    source_to_object_path: input='#{source_path}', clean='#{clean_source}'" if @options[:debug]
    puts "    checking if starts with '#{@options[:src_dir]}/'" if @options[:debug]
    
    # If the source is in our configured source directory, map it to object directory
    if clean_source.start_with?(@options[:src_dir] + '/')
      # Remove src/ prefix and add obj/ prefix: src/main.cpp -> obj/main.o
      relative_path = clean_source.sub(/^#{Regexp.escape(@options[:src_dir])}\//, '')
      object_path = File.join(@options[:obj_dir], relative_path)
      result = Pathname.new(object_path).sub_ext('.o').to_s
      puts "    mapped to: '#{result}'" if @options[:debug]
      result
    else
      # For files not in src/, keep them in the same relative location but change extension
      result = Pathname.new(clean_source).sub_ext('.o').to_s
      puts "    not in src/, mapped to: '#{result}'" if @options[:debug]
      result
    end
  end

  def parse_file(file_path)
    return unless File.exist?(file_path)
    
    @stats[:files_processed] += 1
    puts "Processing: #{file_path}" if @options[:verbose]
    puts ">>> PARSE_FILE DEBUG: Starting parse_file for #{file_path}" if @options[:debug]
    
    begin
      source_file = Pathname.new(file_path)
      imports, exports, includes = extract_dependencies(file_path)
      
      unless imports.empty? && includes.empty?
        object_file = source_file.sub_ext('.o').to_s
        # Normalize path - remove leading ./ if present
        object_file = object_file.sub(/^\.\//, '')
        
        # Add module dependencies
        imports.each do |module_name|
          dep = resolve_module_dependency(module_name)
          if dep
            # Normalize dependency path
            normalized_dep = dep.sub(/^\.\//, '')
            @dependencies[object_file] << normalized_dep
            @module_imports[file_path] << module_name
          end
        end
        
        # Add include dependencies if enabled
        if @options[:include_headers]
          includes.each do |include_file|
            dep = resolve_include_dependency(include_file, file_path)
            if dep
              # Normalize dependency path
              normalized_dep = dep.sub(/^\.\//, '')
              @dependencies[object_file] << normalized_dep
              @include_dependencies[file_path] << include_file
            end
          end
        end
      end
      
      # Track exported modules
      exports.each do |module_name|
        @module_exports[module_name] = file_path
        @stats[:modules_found] += 1
      end
      
    rescue => e
      error_msg = "Error processing #{file_path}: #{e.message}"
      @stats[:errors] << error_msg
      warn error_msg if @options[:verbose]
    end
  end

  def parse_directory(dir_path = '.')
    extensions = %w[.cpp .cc .cxx .c++ .cppm .ccm .cxxm .c++m]
    pattern = File.join(dir_path, '**', "*.{#{extensions.map { |e| e[1..-1] }.join(',')}}")
    
    files = Dir.glob(pattern)
    puts "Found #{files.length} C++ files in #{dir_path}" if @options[:verbose]
    
    files.each { |file| parse_file(file) }
  end

  def extract_dependencies(file_path)
    imports = Set.new
    exports = Set.new
    includes = Set.new
    
    in_multiline_comment = false
    
    File.readlines(file_path, chomp: true).each_with_index do |line, line_num|
      # Handle multi-line comments
      if in_multiline_comment
        if line.include?('*/')
          line = line.split('*/', 2)[1] || ''
          in_multiline_comment = false
        else
          next
        end
      end
      
      if line.include?('/*')
        if line.include?('*/')
          # Single line /* */ comment
          line = line.gsub(%r{/\*.*?\*/}, '')
        else
          # Start of multi-line comment
          line = line.split('/*', 2)[0] || ''
          in_multiline_comment = true
        end
      end
      
      # Remove single-line comments and whitespace
      clean_line = line.gsub(%r{//.*$}, '').strip
      next if clean_line.empty?
      
      # Debug output
      puts "  Line #{line_num + 1}: #{clean_line}" if @options[:debug]
      
      # Match various C++20 module patterns
      case clean_line
      when /^\s*import\s+([a-zA-Z_][a-zA-Z0-9_.:]*)\s*;/
        module_name = $1
        imports << module_name
        @stats[:imports_found] += 1
        puts "    Found import: #{module_name}" if @options[:verbose]
        
      when /^\s*import\s+<([^>]+)>\s*;/
        # System imports like: import <iostream>;
        module_name = $1
        imports << module_name if @options[:include_std]
        @stats[:imports_found] += 1
        puts "    Found system import: #{module_name}" if @options[:verbose]
        
      when /^\s*import\s+"([^"]+)"\s*;/
        # Local imports like: import "my_module";
        module_name = $1
        imports << module_name
        @stats[:imports_found] += 1
        puts "    Found local import: #{module_name}" if @options[:verbose]
        
      when /^\s*export\s+module\s+([a-zA-Z_][a-zA-Z0-9_.:]*)\s*;/
        module_name = $1
        exports << module_name
        puts "    Found export: #{module_name}" if @options[:verbose]
        
      when /^\s*module\s+([a-zA-Z_][a-zA-Z0-9_.:]*)\s*;/
        # Module implementation unit
        module_name = $1
        exports << module_name
        puts "    Found module implementation: #{module_name}" if @options[:verbose]
        
      when /^\s*#include\s+<([^>]+)>/
        # System includes
        include_file = $1
        includes << include_file if @options[:include_headers]
        @stats[:includes_found] += 1
        puts "    Found system include: #{include_file}" if @options[:verbose]
        
      when /^\s*#include\s+"([^"]+)"/
        # Local includes
        include_file = $1
        includes << include_file if @options[:include_headers]
        @stats[:includes_found] += 1
        puts "    Found local include: #{include_file}" if @options[:verbose]
      end
    end
    
    [imports, exports, includes]
  end

  def resolve_module_dependency(module_name)
    puts "    Resolving module dependency: #{module_name}" if @options[:debug]
    
    # First check if we know where this module is exported from
    if @module_exports[module_name]
      source_file = @module_exports[module_name]
      puts "      Found export source: #{source_file}" if @options[:debug]
      # Use our helper method to map the source file to the correct object path
      object_path = source_to_object_path(source_file)
      puts "      Mapped to object: #{object_path}" if @options[:debug]
      return object_path
    end
    
    # Skip standard library modules unless explicitly requested
    return nil if module_name.start_with?('std') && !@options[:include_std]
    
    # Try to find module files with common extensions
    module_extensions = [@options[:module_ext], '.cpp', '.cc', '.cxx']
    
    # Convert module name to possible file paths
    possible_paths = [
      module_name,
      module_name.gsub('.', '/'),
      module_name.gsub('.', '_')
    ]
    
    # Check current directory and subdirectories (prioritizing our source directory)
    found_file = nil
    search_dirs = [@options[:src_dir], 'include', 'lib', '.']
    
    possible_paths.each do |path|
      module_extensions.each do |ext|
        # Check in each search directory
        search_dirs.each do |search_dir|
          candidate_path = if search_dir == '.'
                            "#{path}#{ext}"
                          else
                            File.join(search_dir, "#{path}#{ext}")
                          end
          
          puts "        Checking: #{candidate_path}" if @options[:debug]
          
          if File.exist?(candidate_path)
            found_file = candidate_path
            puts "        Found source file: #{found_file}" if @options[:debug]
            break
          end
        end
        
        break if found_file
      end
      break if found_file
    end
    
    if found_file
      # Use our helper method to convert the found source file to object path
      object_path = source_to_object_path(found_file)
      puts "      Mapped found file to object: #{object_path}" if @options[:debug]
      return object_path
    end
    
    puts "      No module file found for: #{module_name}" if @options[:debug]
    # Only return a guess if we actually found source files with this pattern
    # This prevents creating phantom object files
    return nil
  end

  def resolve_include_dependency(include_file, source_file)
    # Skip system includes
    return nil if include_file.match?(/^[a-z_]+$/) || include_file.include?('/')
    
    # Look for header file relative to source file
    source_dir = File.dirname(source_file)
    possible_paths = [
      File.join(source_dir, include_file),
      include_file
    ]
    
    possible_paths.each do |path|
      if File.exist?(path)
        return path
      end
    end
    
    # Return the include file name if we can't find it
    include_file
  end

  def has_modules?
    @module_exports.any? || @module_imports.any?
  end

  def generate_makefile_fragment
    return '' if @dependencies.empty?
    
    output = []
    output << "# Generated by C++ Module Parser"
    output << "# #{Time.now}"
    output << "# Sources in #{@options[:src_dir]}/, objects in #{@options[:obj_dir]}/"
    output << ""
    
    # First, let's fix the dependencies hash by ensuring all targets use correct object paths
    corrected_dependencies = Hash.new { |h, k| h[k] = Set.new }
    
    @dependencies.each do |target, deps|
      # Find the source file that corresponds to this target
      # The target might be wrong (like src/test.o instead of objects/test.o)
      
      puts "  Processing target: #{target}" if @options[:debug]
      
      # Try to find the source file by checking if any source would map to this target
      # or if we can reverse-engineer the source from the target
      corresponding_source = nil
      
      # First, check if any source file maps to this exact target
      actual_source_files = Dir.glob("**/*.{cpp,cc,cxx,c++,cppm,ccm,cxxm,c++m}")
      corresponding_source = actual_source_files.find do |src|
        source_to_object_path(src) == target
      end
      
      # If not found, try to reverse-engineer by assuming target is incorrectly in src/
      if !corresponding_source && target.start_with?('src/')
        # Convert src/sub/x.o back to src/sub/x.cc and see if that source exists
        possible_source_path = target.sub(/\.o$/, '')
        %w[.cpp .cc .cxx .c++ .cppm .ccm .cxxm .c++m].each do |ext|
          candidate = possible_source_path + ext
          if File.exist?(candidate)
            corresponding_source = candidate
            puts "    Reverse-engineered source: #{candidate}" if @options[:debug]
            break
          end
        end
      end
      
      if corresponding_source
        # Use the correct mapping
        correct_target = source_to_object_path(corresponding_source)
        corrected_dependencies[correct_target].merge(deps)
        puts "    Fixed target: #{target} -> #{correct_target}" if @options[:debug]
      else
        # Keep the original if we can't find a source
        corrected_dependencies[target].merge(deps)
        puts "    Kept target: #{target} (no source found)" if @options[:debug]
      end
    end
    
    # Debug: Show what dependencies we have after correction
    if @options[:debug]
      puts "=== Debug: Corrected dependencies ==="
      corrected_dependencies.each do |target, deps|
        puts "  #{target} -> #{deps.to_a.join(', ')}"
      end
      puts "================================="
    end
    
    # Collect all object files that correspond to actual source files
    all_objects = Set.new
    
    # Find all actual source files and map them to their object file locations
    actual_source_files = Dir.glob("**/*.{cpp,cc,cxx,c++,cppm,ccm,cxxm,c++m}")
    actual_source_files.each do |source_file|
      # Use our helper method to map source to object path (src/ -> obj/)
      object_file = source_to_object_path(source_file)
      all_objects << object_file
      puts "  Source: #{source_file} -> Object: #{object_file}" if @options[:debug]
    end
    
    # Generate OBJECTS variable
    unless all_objects.empty?
      objects_list = all_objects.to_a.sort
      output << "# Object files"
      if objects_list.length <= 5
        output << "OBJECTS = #{objects_list.join(' ')}"
      else
        output << "OBJECTS = \\"
        objects_list.each_with_index do |obj, i|
          if i == objects_list.length - 1
            output << "    #{obj}"
          else
            output << "    #{obj} \\"
          end
        end
      end
      output << ""
    end
    
    # Generate dependencies using the corrected dependencies
    output << "# Dependencies"
    corrected_dependencies.sort.each do |target, deps|
      next if deps.empty?
      
      # Filter out header files - only include .o files (object files)
      object_deps = deps.select { |dep| dep.end_with?('.o') }
      next if object_deps.empty?
      
      deps_list = object_deps.sort.join(' ')
      output << "#{target}: #{deps_list}"
    end
    output << ""
    
    # Module compilation flags (if needed)
    if has_modules?
      output << "# Module compilation flags"
      output << "MODULE_FLAGS = -fmodules-ts"
      output << ""
    end
    
    output.join("\n")
  end

  def generate_advanced_makefile
    output = []
    output << "# Advanced C++ Makefile with Module Support"
    output << "# Generated by C++ Module Parser"
    output << "# #{Time.now}"
    output << ""
    
    # Collect all source and object files
    all_sources = Set.new
    all_objects = Set.new
    source_to_object = {}
    
    # Scan for source files
    extensions = %w[.cpp .cc .cxx .c++ .cppm .ccm .cxxm .c++m]
    extensions.each do |ext|
      Dir.glob("**/*#{ext}").each do |source_file|
        object_file = Pathname.new(source_file).sub_ext('.o').to_s
        all_sources << source_file
        all_objects << object_file
        source_to_object[source_file] = object_file
      end
    end
    
    # Add objects from dependencies
    @dependencies.keys.each { |obj| all_objects << obj }
    
    # Project configuration
    output << "# Project configuration"
    output << "PROJECT_NAME ?= $(notdir $(CURDIR))"
    output << "CXX ?= g++"
    output << "CXXFLAGS ?= -std=c++20 -Wall -Wextra -O2"
    output << "LDFLAGS ?="
    output << "LIBS ?="
    output << ""
    
    # Module-specific configuration
    if has_modules?
      output << "# Module configuration"
      output << "MODULE_FLAGS = -fmodules-ts"
      output << "MODULE_CACHE_DIR = gcm.cache"
      output << "CXXFLAGS += $(MODULE_FLAGS)"
      output << ""
    end
    
    # Source and object lists
    unless all_sources.empty?
      sources_list = all_sources.to_a.sort
      output << "# Source files"
      if sources_list.length <= 3
        output << "SOURCES = #{sources_list.join(' ')}"
      else
        output << "SOURCES = \\"
        sources_list.each_with_index do |src, i|
          if i == sources_list.length - 1
            output << "    #{src}"
          else
            output << "    #{src} \\"
          end
        end
      end
      output << ""
    end
    
    unless all_objects.empty?
      objects_list = all_objects.to_a.sort
      output << "# Object files"
      if objects_list.length <= 3
        output << "OBJECTS = #{objects_list.join(' ')}"
      else
        output << "OBJECTS = \\"
        objects_list.each_with_index do |obj, i|
          if i == objects_list.length - 1
            output << "    #{obj}"
          else
            output << "    #{obj} \\"
          end
        end
      end
      output << ""
    end
    
    # Main targets
    output << "# Main targets"
    output << "all: $(PROJECT_NAME)"
    output << ""
    output << "$(PROJECT_NAME): $(OBJECTS)"
    output << "\t$(CXX) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@"
    output << ""
    
    # Dependencies
    unless @dependencies.empty?
      output << "# Dependencies"
      @dependencies.sort.each do |target, deps|
        next if deps.empty?
        deps_list = deps.to_a.sort.join(' ')
        output << "#{target}: #{deps_list}"
      end
      output << ""
    end
    
    # Module-specific rules
    if has_modules?
      output << "# Module compilation rules"
      output << "%.o: %.cppm"
      output << "\t@mkdir -p $(MODULE_CACHE_DIR)"
      output << "\t$(CXX) $(CXXFLAGS) -c $< -o $@"
      output << ""
      
      output << "%.o: %.ccm"
      output << "\t@mkdir -p $(MODULE_CACHE_DIR)"
      output << "\t$(CXX) $(CXXFLAGS) -c $< -o $@"
      output << ""
      
      output << "%.o: %.cxxm"
      output << "\t@mkdir -p $(MODULE_CACHE_DIR)"
      output << "\t$(CXX) $(CXXFLAGS) -c $< -o $@"
      output << ""
      
      output << "%.o: %.c++m"
      output << "\t@mkdir -p $(MODULE_CACHE_DIR)"
      output << "\t$(CXX) $(CXXFLAGS) -c $< -o $@"
      output << ""
    end
    
    # Standard compilation rules
    output << "# Standard compilation rules"
    output << "%.o: %.cpp"
    output << "\t$(CXX) $(CXXFLAGS) -c $< -o $@"
    output << ""
    
    output << "%.o: %.cc"
    output << "\t$(CXX) $(CXXFLAGS) -c $< -o $@"
    output << ""
    
    output << "%.o: %.cxx"
    output << "\t$(CXX) $(CXXFLAGS) -c $< -o $@"
    output << ""
    
    output << "%.o: %.c++"
    output << "\t$(CXX) $(CXXFLAGS) -c $< -o $@"
    output << ""
    
    # Utility targets
    output << "# Utility targets"
    output << ".PHONY: all clean install debug release help"
    output << ""
    
    output << "clean:"
    output << "\trm -f $(OBJECTS) $(PROJECT_NAME)"
    if has_modules?
      output << "\trm -rf $(MODULE_CACHE_DIR) *.pcm"
    end
    output << ""
    
    output << "debug: CXXFLAGS += -g -DDEBUG"
    output << "debug: $(PROJECT_NAME)"
    output << ""
    
    output << "release: CXXFLAGS += -O3 -DNDEBUG"
    output << "release: $(PROJECT_NAME)"
    output << ""
    
    output << "install: $(PROJECT_NAME)"
    output << "\tinstall -D $(PROJECT_NAME) $(DESTDIR)$(PREFIX)/bin/$(PROJECT_NAME)"
    output << ""
    
    output << "help:"
    output << "\t@echo 'Available targets:'"
    output << "\t@echo '  all      - Build the project (default)'"
    output << "\t@echo '  clean    - Remove built files'"
    output << "\t@echo '  debug    - Build with debug flags'"
    output << "\t@echo '  release  - Build with optimization'"
    output << "\t@echo '  install  - Install the binary'"
    output << "\t@echo '  help     - Show this help'"
    
    output.join("\n") + "\n"
  end

  def generate_detailed_report
    report = []
    report << "=== C++ Module Dependency Analysis Report ==="
    report << "Generated: #{Time.now}"
    report << ""
    
    # Statistics
    report << "Statistics:"
    report << "  Files processed: #{@stats[:files_processed]}"
    report << "  Modules found: #{@stats[:modules_found]}"
    report << "  Imports found: #{@stats[:imports_found]}"
    report << "  Includes found: #{@stats[:includes_found]}"
    report << "  Errors: #{@stats[:errors].length}"
    report << ""
    
    # Module exports
    unless @module_exports.empty?
      report << "Module Exports:"
      @module_exports.sort.each do |module_name, file_path|
        report << "  #{module_name} -> #{file_path}"
      end
      report << ""
    end
    
    # Dependencies
    unless @dependencies.empty?
      report << "Dependencies:"
      @dependencies.sort.each do |target, deps|
        next if deps.empty?
        report << "  #{target}:"
        deps.sort.each do |dep|
          report << "    #{dep}"
        end
      end
      report << ""
    end
    
    # Errors
    unless @stats[:errors].empty?
      report << "Errors:"
      @stats[:errors].each do |error|
        report << "  #{error}"
      end
      report << ""
    end
    
    report.join("\n")
  end

  def self.run(args = ARGV)
    options = {
      verbose: false,
      include_std: false,
      include_headers: true,
      module_ext: '.cppm',
      debug: false,
      output_file: nil,
      makefile_file: nil,
      report_file: nil,
      src_dir: 'src',
      obj_dir: 'obj'
    }
    
    files_to_parse = []
    
    opt_parser = OptionParser.new do |opts|
      opts.banner = "Usage: #{$0} [options] [files/directories...]"
      opts.separator ""
      opts.separator "Options:"
      
      opts.on("-m", "--makefile FILE", "Generate complete Makefile to FILE") do |file|
        options[:makefile_file] = file
      end
      
      opts.on("-o", "--output FILE", "Write Makefile fragment to FILE") do |file|
        options[:output_file] = file
      end
      
      opts.on("-r", "--report FILE", "Write detailed report to FILE") do |file|
        options[:report_file] = file
      end
      
      opts.on("-v", "--verbose", "Verbose output") do
        options[:verbose] = true
      end
      
      opts.on("--debug", "Debug output (very verbose)") do
        options[:debug] = true
        options[:verbose] = true
      end
      
      opts.on("--include-std", "Include standard library dependencies") do
        options[:include_std] = true
      end
      
      opts.on("--no-headers", "Don't process #include dependencies") do
        options[:include_headers] = false
      end
      
      opts.on("--module-ext EXT", "Module file extension (default: .cppm)") do |ext|
        options[:module_ext] = ext.start_with?('.') ? ext : ".#{ext}"
      end
      
      opts.on("--src-dir DIR", "Source directory (default: src)") do |dir|
        options[:src_dir] = dir
      end
      
      opts.on("--obj-dir DIR", "Object directory (default: obj)") do |dir|
        options[:obj_dir] = dir
      end
      
      opts.on("-h", "--help", "Show this help") do
        puts opts
        puts ""
        puts "Examples:"
        puts "  #{$0}                              # Parse current directory"
        puts "  #{$0} src/                         # Parse src directory"
        puts "  #{$0} -o deps.mk src/              # Output fragment to deps.mk"
        puts "  #{$0} -m Makefile src/             # Generate complete Makefile"
        puts "  #{$0} --src-dir source --obj-dir build  # Custom source and object dirs"
        puts "  #{$0} -v -r report.txt src/        # Verbose with detailed report"
        puts "  #{$0} --include-std --debug src/   # Include std modules with debug"
        exit 0
      end
    end
    
    begin
      opt_parser.parse!(args)
      files_to_parse = args
    rescue OptionParser::InvalidOption => e
      puts "Error: #{e.message}"
      puts opt_parser
      exit 1
    end
    
    # Create parser instance
    parser = new(options)
    
    # Parse files or directories
    if files_to_parse.empty?
      puts "Parsing current directory..." if options[:verbose]
      parser.parse_directory('.')
    else
      files_to_parse.each do |arg|
        if File.directory?(arg)
          puts "Parsing directory: #{arg}" if options[:verbose]
          parser.parse_directory(arg)
        elsif File.file?(arg)
          puts "Parsing file: #{arg}" if options[:verbose]
          parser.parse_file(arg)
        else
          warn "Warning: #{arg} is not a valid file or directory"
        end
      end
    end
    
    # Generate and output results
    if options[:makefile_file]
      # Generate complete Makefile
      makefile_content = parser.generate_advanced_makefile
      begin
        FileUtils.mkdir_p(File.dirname(options[:makefile_file]))
        File.write(options[:makefile_file], makefile_content)
        puts "Complete Makefile written to #{options[:makefile_file]}"
      rescue => e
        warn "Error writing Makefile to #{options[:makefile_file]}: #{e.message}"
        exit 1
      end
    elsif options[:output_file]
      # Generate dependency fragment
      fragment = parser.generate_makefile_fragment
      begin
        FileUtils.mkdir_p(File.dirname(options[:output_file]))
        File.write(options[:output_file], fragment)
        puts "Makefile fragment written to #{options[:output_file]}"
      rescue => e
        warn "Error writing to #{options[:output_file]}: #{e.message}"
        exit 1
      end
    else
      # Output to stdout
      fragment = parser.generate_makefile_fragment
      puts fragment
    end
    
    # Generate detailed report if requested
    if options[:report_file]
      begin
        FileUtils.mkdir_p(File.dirname(options[:report_file]))
        File.write(options[:report_file], parser.generate_detailed_report)
        puts "Detailed report written to #{options[:report_file]}"
      rescue => e
        warn "Error writing report to #{options[:report_file]}: #{e.message}"
        exit 1
      end
    elsif options[:verbose]
      puts ""
      puts parser.generate_detailed_report
    end
    
    # Show summary
    stats = parser.stats
    puts "Summary: #{stats[:files_processed]} files, #{stats[:modules_found]} modules, #{stats[:imports_found]} imports"
    puts "Errors: #{stats[:errors].length}" if stats[:errors].length > 0
  end
end

# Command line interface
if __FILE__ == $0
  CppModuleParser.run(ARGV)
end