# Kakiage Template Engine

A powerful and flexible template engine written in C++17.

The name "Kakiage" is a play on words combining:
- **かき揚げ (Kakiage)** - A type of Japanese tempura (deep-fried food)
- **書き上げ (Kakiage)** - Japanese phrase meaning "to write up" or "to complete writing"

## Features

- **Variable Substitution** - Simple `{{.variable}}` syntax
- **Conditional Directives** - `#if`, `#elif`, `#else` for logic control
- **Macro System** - Define and reuse template fragments with `#define` and `#put`
- **File Inclusion** - Include external templates and files
- **Command Execution** - Execute shell commands and embed results
- **Encoding Support** - Built-in HTML and URL encoding
- **Environment Variables** - Access system environment variables
- **Comment Support** - Template comments that don't appear in output
- **Escape Sequences** - Escape special characters when needed

## Building

### Requirements

- C++17 compatible compiler
- Qt/qmake build system
- OpenSSL library (for HTTPS support)

### Linux

```bash
qmake
make
```

The binary will be created in the `out/` directory.

### macOS

```bash
qmake
make
```

### Windows (MSVC)

Ensure OpenSSL is installed and paths are configured in the `.pro` file, then:

```bash
qmake
nmake
```

## Usage

### Basic Command Line

```bash
# Process a template file
kakiage input.tmpl

# Process with output file
kakiage input.tmpl -o output.html

# Process inline template string
kakiage -s "Hello {{.name}}"

# Load definitions from file
kakiage input.tmpl -d definitions.ka

# Define variables on command line
kakiage input.tmpl -D name=value -D foo=bar

# Enable HTML mode (auto-encode variables)
kakiage input.tmpl --html
```

### Definition File Format

Definition files (`.ka` files) use simple `key=value` format:

```
name=John Doe
age=30
city=Tokyo
# Comments start with # or ;
email=john@example.com
```

## Template Syntax

### Variable Substitution

The most basic operation is variable substitution:

```
Hello {{.name}}!
Your age is {{.age}}.
```

With definitions `name=John` and `age=25`, this produces:
```
Hello John!
Your age is 25.
```

### Comments

Comments are removed from the output:

```
{{.;This is a comment and will not appear in output}}
Hello {{.name}}{{.;another comment}}!
```

Produces:
```
Hello John!
```

## Directives

Directives start with `#` inside the template syntax `{{.#directive...}}`.

### #define - Define Macros

Define reusable template fragments (macros).

**Syntax variants:**

```
{{.#define.name=value}}
{{.#define.name value}}
{{.#define.name  multiword value}}
{{.#define('name','value')}}
{{.#define('name', 'value')}}
{{.#define("name","value")}}
{{.#define("name", "value")}}
```

**Examples:**

```
{{.#define.copyright=Copyright 2024 ACME Corp}}
{{.#define.title My Awesome Page}}
{{.#define('version','1.0.0')}}

Footer: {{.#put.copyright}}
```

Macros can contain template syntax:

```
{{.#define.greeting=Hello {{.name}}!}}
{{.#put.greeting}}
```

**Note:** `#define` directives consume the trailing newline if present.

### #put - Output Macros

Output the value of a defined macro or call a custom evaluator function.

**Syntax variants:**

```
{{.#put.name}}
{{.#put('name')}}
{{.#put("name")}}
{{.#put.function_name}}
{{.#put('function_name')}}
```

**Examples:**

```
{{.#define.site_name=Kakiage Project}}
Website: {{.#put.site_name}}

{{.#put.inet_checkip}}  <!-- Calls custom evaluator function -->
{{.#put.inet_resolve("a.root-servers.net")}}
```

If a macro is undefined, it will output `?name?` and print an error to stderr.

### #if, #elif, #else, #end - Conditional Processing

Conditionally include template sections based on numeric values (0 = false, non-zero = true).

**Syntax:**

```
{{.#if.condition}}
  content when true
{{.#elif.other_condition}}
  content when first is false but this is true
{{.#else}}
  content when all are false
{{.#end}}
```

The `{{.}}` end marker can be used instead of `{{.#end}}`:

```
{{.#if.condition}}
  content
{{.}}
```

**Examples:**

```
{{.#if.1}}
  This will be included (1 is true)
{{.}}

{{.#if.0}}
  This will be skipped (0 is false)
{{.#else}}
  This will be included
{{.}}

{{.#if.debug_mode}}
  Debug information here
{{.}}

{{.#if.user_type}}
  {{.#elif.guest_mode}}
    Guest content
  {{.#else}}
    Default content
  {{.}}
```

### #ifn, #elifn - Negated Conditionals

Inverse of `#if` and `#elif` - true when value is 0.

**Examples:**

```
{{.#ifn.0}}
  This is included (0 is true for ifn)
{{.}}

{{.#ifn.production}}
  Development mode content
{{.}}
```

### #include - Include External Templates

Include and process external template files.

**Syntax:**

```
{{.#include.filename.txt}}
{{.#include("filename.txt")}}
```

**Example:**

If `header.html` contains:
```html
<header><h1>{{.site_name}}</h1></header>
```

Then:
```
{{.#include("header.html")}}
<main>Content here</main>
```

Produces:
```html
<header><h1>My Site</h1></header>
<main>Content here</main>
```

**Note:** The includer function must be implemented by the host application. Include depth is limited to 10 levels to prevent infinite recursion.

### #html - HTML Encode

Output HTML-encoded value (safe for HTML content).

**Syntax:**

```
{{.#html.variable}}
{{.#html("string")}}
{{.#html(<file.txt>)}}
```

**Examples:**

```
{{.#html.user_input}}
<!-- <script> becomes &lt;script&gt; -->

{{.#html("<test>")}}
<!-- outputs: &lt;test&gt; -->

{{.#html(<file.txt>)}}
<!-- outputs HTML-encoded contents of file.txt -->
```

### #url - URL Encode

Output URL-encoded value (safe for URLs).

**Syntax:**

```
{{.#url.variable}}
{{.#url("string")}}
{{.#url(<file.txt>)}}
```

**Examples:**

```
<a href="/search?q={{.#url.search_term}}">Search</a>

{{.#url("hello world")}}
<!-- outputs: hello%20world -->

{{.#url(<file.txt>)}}
<!-- outputs URL-encoded contents of file.txt -->
```

### #raw - Raw Output

Output raw value without any processing or encoding (same as `{{.variable}}`).

**Syntax:**

```
{{.#raw.variable}}
{{.#raw(variable)}}
```

**Example:**

```
{{.#raw.html_content}}
<!-- outputs value as-is, no encoding -->
```

### #for - Custom Loop Handler

Calls a custom evaluator function for loop processing.

**Syntax:**

```
{{.#for.iterator_name value}}
```

**Note:** The `#for` directive requires a custom evaluator function to be implemented by the host application. The exact behavior depends on the implementation.

## Special Syntax

### Command Execution

Execute shell commands and embed their output:

```
{{.`command`}}
```

**Examples:**

```
System: {{.`uname`}}
<!-- outputs: System: Linux -->

Current user: {{.`whoami`}}

Files: {{.`ls -la`}}
```

The command output is trimmed (leading/trailing whitespace removed).

### Environment Variables

Access system environment variables:

```
{{.$(VARIABLE_NAME)}}
```

**Examples:**

```
Shell: {{.$(SHELL)}}
<!-- outputs: Shell: /bin/bash -->

Home: {{.$(HOME)}}

Path: {{.$(PATH)}}
```

### File Inclusion (Angle Brackets)

Include file contents directly:

```
{{.<filename>}}
```

**Example:**

```
{{.<config.txt>}}
<!-- includes the contents of config.txt -->
```

**Note:** This requires the includer function to be implemented by the host application.

### String Formatting

Format strings with arguments using printf-style syntax:

```
{{.%(format, arg1, arg2, ...)}}
```

**Example:**

```
{{.%("User %s has %d points", name, score)}}
```

**Note:** This feature uses the `strformat.h` implementation.

## Escape Sequences

Escape special characters that would otherwise be interpreted as template syntax:

| Sequence | Outputs | Description |
|----------|---------|-------------|
| `&.;` | `.` | Literal dot |
| `&{;` | `{` | Literal opening brace |
| `&};` | `}` | Literal closing brace |
| `&&;` | `&` | Literal ampersand |

**Examples:**

```
Use &{;&{;.variable&};&}; to output a variable
<!-- outputs: Use {{.variable}} to output a variable -->

To escape, use &&;
<!-- outputs: To escape, use & -->
```

The escape sequence must end with `;` before a newline, or the literal character is output instead.

## Advanced Examples

### Website Template

```html
{{.#define.site_name=My Website}}
{{.#define.copyright=Copyright 2024 ACME Corp}}

<!DOCTYPE html>
<html>
<head>
    <title>{{.#put.site_name}} - {{.page_title}}</title>
</head>
<body>
    <header>
        <h1>{{.#put.site_name}}</h1>
    </header>

    <main>
        {{.#if.is_logged_in}}
            <p>Welcome, {{.username}}!</p>
        {{.#else}}
            <p>Please log in.</p>
        {{.}}

        {{.#include("content.html")}}
    </main>

    <footer>
        <p>{{.#put.copyright}}</p>
        <p>Generated on {{.`date`}}</p>
    </footer>
</body>
</html>
```

### Configuration File Generator

```bash
# Config file for {{.app_name}}
# Generated: {{.`date`}}

SERVER_HOST={{.server_host}}
SERVER_PORT={{.server_port}}

{{.#if.enable_ssl}}
SSL_ENABLED=true
SSL_CERT={{.ssl_cert_path}}
SSL_KEY={{.ssl_key_path}}
{{.#else}}
SSL_ENABLED=false
{{.}}

DATABASE_URL={{.#url.db_connection_string}}

{{.#if.debug}}
LOG_LEVEL=DEBUG
{{.#else}}
LOG_LEVEL=INFO
{{.}}
```

### Code Generation

```cpp
// Auto-generated file - DO NOT EDIT
// Generated: {{.`date`}}

{{.#define.class_name=DataModel}}

class {{.#put.class_name}} {
public:
    {{.#put.class_name}}() = default;
    ~{{.#put.class_name}}() = default;

    {{.#if.include_getters}}
    {{.;Generate getter methods}}
    std::string getName() const { return name_; }
    int getAge() const { return age_; }
    {{.}}

private:
    std::string name_;
    int age_;
};
```

## Testing

Run the built-in test suite:

```bash
kakiage --test
```

This will run all test cases defined in [main.cpp:209-393](main.cpp#L209-L393) and report results.

## Custom Evaluators

The template engine supports custom evaluator functions for extending functionality. When used as a library, you can register custom functions:

```cpp
kakiage st;
st.evaluator = [](std::string const &name, std::string const &text,
                  std::vector<std::string> const &args) -> std::optional<std::string> {
    if (name == "custom_function") {
        // Your custom logic here
        return "result";
    }
    return std::nullopt;
};
```

## License

(License information not specified in the current codebase)

## Contributing

(Contribution guidelines not specified in the current codebase)

