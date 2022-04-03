import json

with open('Coverage-iframe.json') as json_data:
    raw_code_coverage_list = json.load(json_data)
    # print(data_dict)

file_name = 'bootstrap.min.css'

for raw_code_coverage in raw_code_coverage_list:

    # print(raw_code_coverage.keys())
    # dict_keys(['url', 'ranges', 'text'])
    # print(raw_code_coverage['url'])

    if file_name in raw_code_coverage['url']:

        #print(raw_code_coverage['url'])

        ranges_list = raw_code_coverage['ranges']

        #print(type(ranges_list))
        #print(type(ranges_list[0]))
        #print(ranges)

        code = raw_code_coverage['text']

        code_clean = ''     
        for ranges in ranges_list:
            code_clean += code[ranges['start']:ranges['end']]

f= open("bootstrap.iframe.css","w+")

f.write(code_clean)

f.close()