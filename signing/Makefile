include ./bbpass

KEYSTORE   := ./author.p12
BBIDTOKEN  := ./bbidtoken.csk
DEBUGTOKEN := ./debugtoken.bar
SSHKEY     := ./ssh-key

.PHONY: all author-info deploy-token connect

all: deploy-token

$(DEBUGTOKEN):
ifeq ($(BBPASS),"")
	$(error Debug token error: fill in the necessary fields in ./signing/bbpass))
endif
	@echo Generating keypairs and writing to keystore...
	blackberry-keytool -genkeypair -keystore $(KEYSTORE) -storepass $(KEYSTOREPASS) -author $(CNNAME)
	blackberry-debugtokenrequest -keystore $(KEYSTORE) -bbidtoken $(BBIDTOKEN) -storepass $(KEYSTOREPASS) -deviceID 0x$(BBPIN) $(DEBUGTOKEN)

$(SSHKEY):
	@echo Generating SSH key...
	ssh-keygen -b 4096 -t rsa -f $(SSHKEY) -q -N ""

author-info: $(DEBUGTOKEN)
	@echo
	@echo "Remember to update the bar-descriptor.xml <author> and <authorId> tags:"
	@unzip -p $(DEBUGTOKEN) META-INF/MANIFEST.MF | grep 'Package-Author:\|Package-Author-Id:'
	@echo

deploy-token: $(DEBUGTOKEN) author-info
	blackberry-deploy -installDebugToken $(DEBUGTOKEN) $(BBIP) -password $(BBPASS)
