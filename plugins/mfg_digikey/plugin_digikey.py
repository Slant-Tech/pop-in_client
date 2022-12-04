#! /usr/bin/python
import logging
import os
import digikey
from digikey.v3.productinformation import KeywordSearchRequest

def get_price_breaks(pn):
    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)
    
    digikey_logger = logging.getLogger('digikey')
    digikey_logger.setLevel(logging.DEBUG)
    
    handler = logging.StreamHandler()
    handler.setLevel(logging.DEBUG)
    logger.addHandler(handler)
    digikey_logger.addHandler(handler)
    
    
    os.environ
    
    # Production test
    #os.environ['DIGIKEY_CLIENT_ID'] = 'ROuINEuDVNOhRapzIX1BrdAzqs1xpQDx'
    #os.environ['DIGIKEY_CLIENT_SECRET'] = '29nIAeuzdjkVg58m'
    #os.environ['DIGIKEY_CLIENT_SANDBOX'] = 'False'
    #os.environ['DIGIKEY_STORAGE_PATH']= './cache-production'
    # Sandbox test
    os.environ['DIGIKEY_CLIENT_ID'] = 'Kqfxt7GxxOd5nGCRkegaPcucGuKGpCG7'
    os.environ['DIGIKEY_CLIENT_SECRET'] = 'Edrd9M79MP7schiW'
    os.environ['DIGIKEY_CLIENT_SANDBOX'] = 'True'
    
    os.environ['DIGIKEY_STORAGE_PATH']= '/tmp/cache-sandbox'
    
#    dkpn = '541-4131-1-ND'
    part = digikey.product_details(pn)
    
    print( part.quantity_available )
    print( part.product_status )
    
#    print(f"Return type: {type(part.standard_pricing)}" )
#    print(f"Return index type: {type(part.standard_pricing[0])}" )
#    print(f"break quantity type:{type(part.standard_pricing[0].break_quantity)}" )
#    print(f"unit price type: {type(part.standard_pricing[0].unit_price)}" )

    for price in part.standard_pricing:
        print( f'{price.break_quantity}: {price.unit_price}' )

    return part.standard_pricing
    
    #search_request = KeywordSearchRequest(keywords='CAP;0603;100uF', record_count=10)
    #result = digikey.keyword_search(body=search_request)
    
    #print(result)
