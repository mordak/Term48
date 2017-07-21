include ./bbpass

KEYSTORE   := ./author.p12
BBIDTOKEN  := ./bbidtoken.csk
DEBUGTOKEN := ./debugtoken.bar

all: deploy-token

$(DEBUGTOKEN):
ifeq ($(BBPASS),"")
	$(error Debug token error: fill in the necessary fields in ./signing/bbpass))
endif
	@echo Generating keypairs and writing to keystore...
	@echo blackberry-keytool -genkeypair -keystore $(KEYSTORE) -storepass $(KEYSTOREPASS) -author $(CNNAME)
	blackberry-debugtokenrequest -keystore $(KEYSTORE) -bbidtoken $(BBIDTOKEN) -storepass $(KEYSTOREPASS) -deviceID 0x$(BBPIN) $(DEBUGTOKEN)

author-info: $(DEBUGTOKEN)
	@echo
	@echo "Remember to update the bar-descriptor.xml <author> and <authorId> tags:"
	@unzip -p $(DEBUGTOKEN) META-INF/MANIFEST.MF | grep 'Package-Author:\|Package-Author-Id:'
	@echo

deploy-token: $(DEBUGTOKEN) author-info
	@echo Deploying debugtoken.bar...
	@echo blackberry-deploy -installDebugToken $(DEBUGTOKEN) $(BBIP) -password $(BBPASS)
