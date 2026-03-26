# GitHub repository secrets

In the unfortunate event that the lead developers become unavailable for any
reason, here is documentation detailing the third-party integrations we're
using, including some paid services. All integrations are automated through CI,
and the access information is securely stored in GitHub secrets.

### DockerHub

We utilize DockerHub for our Docker builds, maintaining distinct images for
Windows and Linux platforms. (Mac builds are an exception and do not employ
Docker.) Each image is equipped with the necessary tools to compile the game
executable, thus eliminating the need for developers or CI environments to
install additional tools on their machines. While these images can be built
locally, we host the pre-built Docker images on DockerHub for the convenience
of new developers and CI processes that otherwise couldn't cache the images.

**Variables**:

- `DOCKERHUB_USERNAME`:  
    The username to log to the DockerHub and under which the images are hosted.

- `DOCKERHUB_PASSWORD`:  
    The password to the account.

Whenever we change the images for any reason (such as by introducing a new hard
dependency), we have to run the Build Docker toolchain GitHub action by hand.

### MacOS builds

MacOS builds require a paid Apple Developer account.

**Variables**:

- `MACOS_APPLEID`:  
    Apple developer account id / email address

- `MACOS_APP_PWD`:  
    Apple developer account password. It is recommended to use an app password,
    that can be individually revoked.

- `MACOS_TEAMID`:  
    Every Apple developer account has an associated team ID. To see one:
    1. Navigate to https://developer.apple.com/account.
    2. Go to Membership details.
    3. Examine the `Team ID` value.

- `MACOS_KEYCHAIN_PWD`:  
    This is used for a temporary keychain file made by the GitHub workflow - as
    such, the exact value doesn't really matter.

- `MACOS_CERTIFICATE`:  
    A codesigning certificate generated from the Apple developer account. To generate it:

    1. Navigate to https://developer.apple.com/account/resources/certificates/list.

    2. Create a new Certificate:

       1. Select Developer ID Application; continue to the next page.

       2. Create a Certificate Signing Request and Private Key pair:

           > openssl req -new -newkey rsa:2048 -nodes -keyout TR1X.key -out TR1X.csr -subj "/emailAddress=your-mail@example.com, CN=TR1X"

       3. Upload the newly generated `TR1X.csr` file; continue to the next page.

    3. Download the certificate and save it as `TR1X.cer`.

    4. Convert the certificate to the PKCS12 format - run:

       > openssl pkcs12 -export -out TR1X.pem -inkey TR1X.key -in TR1X.cer -name TR1X -legacy

       This command will ask you for a password. It should be noted down.

    5. Serialize the key in base-64 without spaces - run:

       > base64 TR1X.pem|tr -d '\n'
       > base64 -i BUILD_CERTIFICATE.p12 | pbcopy (macos)

       The result is to be put as the value of the `MACOS_CERTIFICATE` secret.

- `MACOS_CERTIFICATE_PWD`:  
    The password to the `MACOS_CERTIFICATE`.
