import argparse
import sys
import jsonschema
import json
def handle_validation_error():
    sys.exit("validation failed");
def validate_schema(data,schema):
    with open(data) as data_handle:
        data_json=json.load(data_handle)
        with open(schema) as schema_handle:
            schema_json=json.load(schema_handle)
            try:
               jsonschema.validate(data_json, schema_json)
            except jsonschema.ValidationError as e:
                print(e)
                handle_validation_error()
    return data_json
if __name__ == '__main__':
    parser=argparse.ArgumentParser(description='Vpd inventory JSON validator')
    parser.add_argument('-s','--json_schema',dest='json_schema',help='JSON schema for vpd inventory JSONs.')
    parser.add_argument('-d','--json_doc',dest='json_doc',help='Vpd inventory JSON document')
    args=parser.parse_args()
    if not args.json_schema:
        parser.print_help()
        sys.exit("Error: Vpd inventory JSON schema is required.")
    if not args.json_doc:
        parser.print_help()
        sys.exit("Error: Vpd inventory JSON document is required.")
    data_json = validate_schema(args.json_doc, args.json_schema)
    print("Validation Success")

