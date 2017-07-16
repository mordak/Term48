include ./bbpass

KEYSTORE   := ./author.p12
BBIDTOKEN  := ./bbidtoken.csk
DEBUGTOKEN := ./debugtoken.bar

all: deploy-token

$(DEBUGTOKEN):
ifeq ($(BBPASS),"")
	$(error Debug token error: fill in the necessary fields in ./signing/bbpass))
endif
	blackberry-keytool -genkeypair -keystore $(KEYSTORE) -storepass $(KEYSTOREPASS) -author $(CNNAME)
	blackberry-debugtokenrequest -keystore $(KEYSTORE) -bbidtoken $(BBIDTOKEN) -storepass $(KEYSTOREPASS) -deviceID 0x$(BBPIN) $(DEBUGTOKEN)

deploy-token: $(DEBUGTOKEN)
	blackberry-deploy -installDebugToken $(DEBUGTOKEN) $(BBIP) -password $(BBPASS)
